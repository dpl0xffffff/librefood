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

Product *parse_product_json(JsonObject *root, const char *barcode) {
    if (!json_object_has_member(root, "product"))
        return NULL;

    JsonObject *prod = json_object_get_object_member(root, "product");
    if (!prod) return NULL;

    Product *p = g_new0(Product, 1);
    p->barcode = g_strdup(barcode);

#define STR_FIELD(obj, key) \
    (json_object_has_member(obj, key) ? \
     g_strdup(json_object_get_string_member(obj, key)) : g_strdup(""))

    p->product_name  = STR_FIELD(prod, "product_name");
    p->brand         = STR_FIELD(prod, "brands");
    p->image_url     = STR_FIELD(prod, "image_front_url");
    p->ingredients_text = STR_FIELD(prod, "ingredients_text");

    /* Nutriscore */
    if (json_object_has_member(prod, "nutriscore_grade"))
        p->nutriscore_grade = g_strdup(json_object_get_string_member(prod, "nutriscore_grade"));

    /* NOVA */
    if (json_object_has_member(prod, "nova_group")) {
        JsonNode *nova_node = json_object_get_member(prod, "nova_group");
        if (JSON_NODE_HOLDS_VALUE(nova_node)) {
            /* nova_group can be int or string */
            if (json_node_get_value_type(nova_node) == G_TYPE_INT64) {
                p->nova_group = g_strdup_printf("%lld",
                    (long long)json_node_get_int(nova_node));
            } else {
                p->nova_group = g_strdup(json_node_get_string(nova_node));
            }
        }
    }

    /* Nutriments */
    if (json_object_has_member(prod, "nutriments")) {
        JsonObject *nm = json_object_get_object_member(prod, "nutriments");
#define NM_DBL(key) \
        (json_object_has_member(nm, key) ? \
         json_object_get_double_member(nm, key) : 0.0)

        p->energy_kcal  = NM_DBL("energy-kcal_100g");
        p->fat          = NM_DBL("fat_100g");
        p->saturated_fat= NM_DBL("saturated-fat_100g");
        p->carbs        = NM_DBL("carbohydrates_100g");
        p->sugars       = NM_DBL("sugars_100g");
        p->fiber        = NM_DBL("fiber_100g");
        p->protein      = NM_DBL("proteins_100g");
        p->salt         = NM_DBL("salt_100g");
    }

    /* Additives */
    if (json_object_has_member(prod, "additives_tags")) {
        JsonArray *arr = json_object_get_array_member(prod, "additives_tags");
        guint len = json_array_get_length(arr);
        p->additives = g_new0(char *, len + 1);
        p->additives_count = (int)len;
        for (guint i = 0; i < len; i++) {
            const char *tag = json_array_get_string_element(arr, i);
            /* strip "en:" prefix */
            const char *colon = strchr(tag, ':');
            p->additives[i] = g_strdup(colon ? colon + 1 : tag);
        }
    }

    /* Fallback name */
    if (p->product_name[0] == '\0') {
        g_free(p->product_name);
        p->product_name = g_strdup("Unknown product");
    }

    return p;
}
