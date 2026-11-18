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
    g_free(p->quantity);
    g_free(p->ecoscore_grade);
    g_strfreev(p->additives);
    g_strfreev(p->allergens);
    g_strfreev(p->traces);
    g_strfreev(p->labels);
    g_clear_object(&p->image);
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

/* Eco-Score (now "Green-Score" on OFF; the API field is still ecoscore_*). */
const char *ecoscore_color(const char *grade) {
    if (!grade) return "ns-score-unknown";
    if (strcmp(grade, "a-plus") == 0) return "ns-score-aplus";
    return score_color(grade);
}

const char *ecoscore_label(const char *grade) {
    if (!grade) return "?";
    if (strcmp(grade, "a-plus") == 0) return "A+";
    static const char *letters[] = { "A", "B", "C", "D", "E", "F" };
    if (grade[0] >= 'a' && grade[0] <= 'f' && grade[1] == '\0')
        return letters[grade[0] - 'a'];
    return "?";
}

const char *ecoscore_description(const char *grade) {
    if (!grade) return "Unknown impact";
    if (strcmp(grade, "a-plus") == 0) return "Very low impact";
    switch (grade[0]) {
        case 'a': return "Very low impact";
        case 'b': return "Low impact";
        case 'c': return "Moderate impact";
        case 'd': return "High impact";
        case 'e': return "Very high impact";
        case 'f': return "Very high impact";
        default:  return "Unknown impact";
    }
}

char *tag_display_name(const char *tag) {
    if (!tag) return g_strdup("");
    const char *colon = strchr(tag, ':');
    const char *name = colon ? colon + 1 : tag;
    char *out = g_strdup(name);
    for (char *c = out; *c; c++)
        if (*c == '-' || *c == '_') *c = ' ';
    if (out[0]) out[0] = g_ascii_toupper(out[0]);
    return out;
}

char *format_amount(double v) {
    if (isnan(v)) return g_strdup("—");
    if (v >= 100.0) return g_strdup_printf("%.0f", v);
    if (v >= 10.0 || v == 0.0) {
        char *s = g_strdup_printf("%.1f", v);
        if (g_str_has_suffix(s, ".0")) s[strlen(s) - 2] = '\0';
        return s;
    }
    char *s = g_strdup_printf("%.2f", v);
    /* trim trailing zeros: 6.30 → 6.3, 2.00 → 2 */
    char *end = s + strlen(s) - 1;
    while (*end == '0') *end-- = '\0';
    if (*end == '.') *end = '\0';
    return s;
}

/* Bar fill for a nutrient row: the "high" threshold sits at 2/3 of the bar,
 * so LOW/MOD/HIGH map to visibly distinct lengths. NAN → 0. */
double nutrient_fraction(const char *nutrient, double per100g) {
    if (!nutrient || isnan(per100g)) return 0.0;
    double high;
    if      (strcmp(nutrient, "fat") == 0)           high = 17.5;
    else if (strcmp(nutrient, "saturated_fat") == 0) high = 5.0;
    else if (strcmp(nutrient, "sugars") == 0)        high = 22.5;
    else if (strcmp(nutrient, "salt") == 0)          high = 1.5;
    else return 0.0;
    double f = per100g / (high * 1.5);
    return f < 0.0 ? 0.0 : (f > 1.0 ? 1.0 : f);
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

/* Reads an array of "xx:tag" strings into a NULL-terminated list, skipping
 * non-strings and duplicates. pretty: "No gluten" vs raw "no-gluten". */
#define LF_MAX_TAGS 64
static char **tag_list(JsonObject *o, const char *key, gboolean pretty) {
    GPtrArray *out = g_ptr_array_new();
    JsonArray *arr = array_member(o, key);
    guint len = arr ? json_array_get_length(arr) : 0;
    for (guint i = 0; i < len && out->len < LF_MAX_TAGS; i++) {
        JsonNode *el = json_array_get_element(arr, i);
        if (!el || !JSON_NODE_HOLDS_VALUE(el) ||
            json_node_get_value_type(el) != G_TYPE_STRING)
            continue;
        const char *tag = json_node_get_string(el);
        if (!tag || !*tag) continue;
        const char *colon = strchr(tag, ':');
        char *v = pretty ? tag_display_name(tag) : g_strdup(colon ? colon + 1 : tag);
        if (!v[0] || g_ptr_array_find_with_equal_func(out, v, g_str_equal, NULL)) {
            g_free(v);
            continue;
        }
        g_ptr_array_add(out, v);
    }
    g_ptr_array_add(out, NULL);
    return (char **)g_ptr_array_free(out, FALSE);
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

    p->quantity = dup_string_member(prod, "quantity");

    const char *eco = string_member(prod, "ecoscore_grade");
    if (eco && (strcmp(eco, "a-plus") == 0 ||
                (strlen(eco) == 1 && eco[0] >= 'a' && eco[0] <= 'f')))
        p->ecoscore_grade = g_strdup(eco);

    p->additives = tag_list(prod, "additives_tags", FALSE);
    p->additives_count = (int)g_strv_length(p->additives);
    p->allergens = tag_list(prod, "allergens_tags", TRUE);
    p->traces    = tag_list(prod, "traces_tags", TRUE);
    p->labels    = tag_list(prod, "labels_tags", TRUE);

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
