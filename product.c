#include "librefood.h"

void product_free(Product *p) {
    if (!p) return;
    g_free(p->barcode);
    g_free(p->product_name);
    g_free(p->brand);
    g_free(p->image_url);
    g_free(p->nutriscore_grade);
    g_free(p->ingredients_text);
    g_free(p->nova_group);
    if (p->additives) {
        for (int i = 0; p->additives[i]; i++)
            g_free(p->additives[i]);
        g_free(p->additives);
    }
    g_free(p);
}

const char *score_color(const char *grade) {
    if (!grade) return "ns-score-unknown";
    switch (grade[0]) {
        case 'a': case 'A': return "ns-score-a";
        case 'b': case 'B': return "ns-score-b";
        case 'c': case 'C': return "ns-score-c";
        case 'd': case 'D': return "ns-score-d";
        case 'e': case 'E': return "ns-score-e";
        default:            return "ns-score-unknown";
    }
}

const char *score_description(const char *grade) {
    if (!grade) return "Unknown";
    switch (grade[0]) {
        case 'a': case 'A': return "Excellent";
        case 'b': case 'B': return "Good";
        case 'c': case 'C': return "Moderate";
        case 'd': case 'D': return "Poor";
        case 'e': case 'E': return "Bad";
        default:            return "Unknown";
    }
}

const char *nova_description(const char *nova) {
    if (!nova) return "Unknown processing";
    switch (nova[0]) {
        case '1': return "Unprocessed food";
        case '2': return "Culinary ingredient";
        case '3': return "Processed food";
        case '4': return "Ultra-processed";
        default:  return "Unknown processing";
    }
}

/* Simple per-100g level thresholds (EU traffic light) */
const char *nutrient_level(const char *nutrient, double per100g) {
    if (!nutrient || isnan(per100g)) return NULL;
    if (strcmp(nutrient, "fat") == 0) {
        if (per100g <= 3.0) return "LOW";
        if (per100g <= 17.5) return "MOD";
        return "HIGH";
    }
    if (strcmp(nutrient, "saturated_fat") == 0) {
        if (per100g <= 1.5) return "LOW";
        if (per100g <= 5.0) return "MOD";
        return "HIGH";
    }
    if (strcmp(nutrient, "sugars") == 0) {
        if (per100g <= 5.0) return "LOW";
        if (per100g <= 22.5) return "MOD";
        return "HIGH";
    }
    if (strcmp(nutrient, "salt") == 0) {
        if (per100g <= 0.3) return "LOW";
        if (per100g <= 1.5) return "MOD";
        return "HIGH";
    }
    return NULL;
}

/* ── Type-tolerant JSON accessors ──
 * OpenFoodFacts data is crowd-sourced: fields are frequently null, missing,
 * or of the wrong type (numbers as strings and vice versa). The plain
 * json_object_get_*_member() calls emit criticals and return NULL/0 on
 * mismatches, which previously led to NULL dereferences. */

static JsonNode *value_member(JsonObject *o, const char *key) {
    if (!o) return NULL;
    JsonNode *n = json_object_get_member(o, key);
    return (n && JSON_NODE_HOLDS_VALUE(n)) ? n : NULL;
}

static JsonObject *object_member(JsonObject *o, const char *key) {
    if (!o) return NULL;
    JsonNode *n = json_object_get_member(o, key);
    return (n && JSON_NODE_HOLDS_OBJECT(n)) ? json_node_get_object(n) : NULL;
}

static JsonArray *array_member(JsonObject *o, const char *key) {
    if (!o) return NULL;
    JsonNode *n = json_object_get_member(o, key);
    return (n && JSON_NODE_HOLDS_ARRAY(n)) ? json_node_get_array(n) : NULL;
}

/* Returns the member's string value, or NULL if absent / not a string. */
static const char *string_member(JsonObject *o, const char *key) {
    JsonNode *n = value_member(o, key);
    if (!n || json_node_get_value_type(n) != G_TYPE_STRING) return NULL;
    return json_node_get_string(n);
}

static char *dup_string_member(JsonObject *o, const char *key) {
    const char *s = string_member(o, key);
    return g_strdup(s ? s : "");
}

/* Parses a number given as int, double or numeric string. */
static gboolean number_member(JsonObject *o, const char *key, double *out) {
    JsonNode *n = value_member(o, key);
    if (!n) return FALSE;
    GType t = json_node_get_value_type(n);
    double v;
    if (t == G_TYPE_INT64 || t == G_TYPE_DOUBLE) {
        v = json_node_get_double(n);
    } else if (t == G_TYPE_STRING) {
        const char *s = json_node_get_string(n);
        char *end = NULL;
        if (!s || !*s) return FALSE;
        v = g_ascii_strtod(s, &end);
        if (end == s || *end != '\0') return FALSE;
    } else {
        return FALSE;
    }
    if (!isfinite(v)) return FALSE;
    *out = v;
    return TRUE;
}

/* Nutriment amounts: unknown, non-numeric or negative values → NAN. */
static double nutriment(JsonObject *nm, const char *key) {
    double v;
    if (!number_member(nm, key, &v) || v < 0.0) return NAN;
    return v;
}

Product *parse_product_json(JsonObject *root, const char *barcode) {
    if (!root) return NULL;

    /* OFF signals "found" with status == 1 (int, occasionally a string). */
    double status = 0;
    if (!number_member(root, "status", &status) || status != 1.0)
        return NULL;

    JsonObject *prod = object_member(root, "product");
    if (!prod) return NULL;

    Product *p = g_new0(Product, 1);
    p->barcode = g_strdup(barcode ? barcode : "");

    p->product_name     = dup_string_member(prod, "product_name");
    p->brand            = dup_string_member(prod, "brands");
    p->image_url        = dup_string_member(prod, "image_front_url");
    p->ingredients_text = dup_string_member(prod, "ingredients_text");

    /* Nutri-Score: keep only real grades; OFF also sends "unknown",
     * "not-applicable", etc. */
    const char *grade = string_member(prod, "nutriscore_grade");
    if (grade && strlen(grade) == 1 &&
        g_ascii_tolower(grade[0]) >= 'a' && g_ascii_tolower(grade[0]) <= 'e') {
        p->nutriscore_grade = g_ascii_strdown(grade, -1);
    }

    /* NOVA: int, double or string; keep only 1..4. */
    double nova;
    if (number_member(prod, "nova_group", &nova) &&
        nova == floor(nova) && nova >= 1.0 && nova <= 4.0) {
        p->nova_group = g_strdup_printf("%d", (int)nova);
    }

    /* Nutriments (missing object → every value NAN) */
    JsonObject *nm = object_member(prod, "nutriments");
    p->energy_kcal   = nutriment(nm, "energy-kcal_100g");
    p->fat           = nutriment(nm, "fat_100g");
    p->saturated_fat = nutriment(nm, "saturated-fat_100g");
    p->carbs         = nutriment(nm, "carbohydrates_100g");
    p->sugars        = nutriment(nm, "sugars_100g");
    p->fiber         = nutriment(nm, "fiber_100g");
    p->protein       = nutriment(nm, "proteins_100g");
    p->salt          = nutriment(nm, "salt_100g");

    /* Additives: skip any non-string elements */
    JsonArray *arr = array_member(prod, "additives_tags");
    guint len = arr ? json_array_get_length(arr) : 0;
    p->additives = g_new0(char *, len + 1);
    int count = 0;
    for (guint i = 0; i < len; i++) {
        JsonNode *el = json_array_get_element(arr, i);
        if (!el || !JSON_NODE_HOLDS_VALUE(el) ||
            json_node_get_value_type(el) != G_TYPE_STRING)
            continue;
        const char *tag = json_node_get_string(el);
        if (!tag || !*tag) continue;
        /* strip "en:" prefix */
        const char *colon = strchr(tag, ':');
        p->additives[count++] = g_strdup(colon ? colon + 1 : tag);
    }
    p->additives_count = count;

    /* Fallback name */
    if (p->product_name[0] == '\0') {
        g_free(p->product_name);
        p->product_name = g_strdup("Unknown product");
    }

    return p;
}

char *barcode_normalize(const char *input) {
    if (!input) return NULL;
    GString *out = g_string_new(NULL);
    for (const char *c = input; *c; c++) {
        if (g_ascii_isspace(*c) || *c == '-') continue;
        if (!g_ascii_isdigit(*c) || out->len >= LF_BARCODE_MAX_LEN) {
            g_string_free(out, TRUE);
            return NULL;
        }
        g_string_append_c(out, *c);
    }
    if (out->len < LF_BARCODE_MIN_LEN) {
        g_string_free(out, TRUE);
        return NULL;
    }
    return g_string_free(out, FALSE);
}
