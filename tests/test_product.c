/*
 * Regression tests for OpenFoodFacts response parsing and barcode input
 * validation. Run via tests/run_tests.sh (uses G_DEBUG=fatal-criticals so
 * any GLib/json-glib critical warning counts as a failure).
 */
#include "../librefood.h"

static Product *parse(const char *json) {
    JsonParser *parser = json_parser_new();
    GError *err = NULL;
    g_assert_true(json_parser_load_from_data(parser, json, -1, &err));
    g_assert_no_error(err);
    JsonNode *root = json_parser_get_root(parser);
    Product *p = parse_product_json(
        JSON_NODE_HOLDS_OBJECT(root) ? json_node_get_object(root) : NULL,
        "12345678");
    g_object_unref(parser);
    return p;
}

static void test_full_product(void) {
    Product *p = parse(
        "{\"status\":1,\"product\":{"
        "\"product_name\":\"Nutella\",\"brands\":\"Ferrero\","
        "\"image_front_url\":\"https://example.org/x.jpg\","
        "\"nutriscore_grade\":\"e\",\"nova_group\":4,"
        "\"ingredients_text\":\"sugar, palm oil\","
        "\"nutriments\":{\"energy-kcal_100g\":539,\"fat_100g\":30.9,"
        "\"salt_100g\":0.107,\"sugars_100g\":56.3},"
        "\"additives_tags\":[\"en:e322\",\"en:e476\"]}}");
    g_assert_nonnull(p);
    g_assert_cmpstr(p->product_name, ==, "Nutella");
    g_assert_cmpstr(p->brand, ==, "Ferrero");
    g_assert_cmpstr(p->nutriscore_grade, ==, "e");
    g_assert_cmpstr(p->nova_group, ==, "4");
    g_assert_cmpfloat_with_epsilon(p->energy_kcal, 539.0, 1e-9);
    g_assert_cmpfloat_with_epsilon(p->fat, 30.9, 1e-9);
    g_assert_true(isnan(p->protein));   /* missing → unknown, not 0 */
    g_assert_cmpint(p->additives_count, ==, 2);
    g_assert_cmpstr(p->additives[0], ==, "e322");
    g_assert_null(p->additives[2]);
    product_free(p);
}

/* OFF returns null for many fields; previously crashed on product_name[0]. */
static void test_null_fields(void) {
    Product *p = parse(
        "{\"status\":1,\"product\":{\"product_name\":null,\"brands\":null,"
        "\"nutriscore_grade\":null,\"nova_group\":null,\"nutriments\":null,"
        "\"additives_tags\":null}}");
    g_assert_nonnull(p);
    g_assert_cmpstr(p->product_name, ==, "Unknown product");
    g_assert_cmpstr(p->brand, ==, "");
    g_assert_null(p->nutriscore_grade);
    g_assert_null(p->nova_group);
    g_assert_true(isnan(p->energy_kcal));
    g_assert_cmpint(p->additives_count, ==, 0);
    product_free(p);
}

/* Wrong JSON types everywhere: numbers as strings, strings as numbers, etc. */
static void test_wrong_types(void) {
    Product *p = parse(
        "{\"status\":\"1\",\"product\":{\"product_name\":42,"
        "\"brands\":[\"a\"],\"nutriscore_grade\":\"unknown\","
        "\"nova_group\":\"3\","
        "\"nutriments\":{\"fat_100g\":\"12.5\",\"salt_100g\":\"abc\","
        "\"sugars_100g\":true,\"proteins_100g\":-3},"
        "\"additives_tags\":[\"en:e100\",7,null,\"e200\"]}}");
    g_assert_nonnull(p);
    g_assert_cmpstr(p->product_name, ==, "Unknown product");
    g_assert_null(p->nutriscore_grade);          /* "unknown" is not a grade */
    g_assert_cmpstr(p->nova_group, ==, "3");
    g_assert_cmpfloat_with_epsilon(p->fat, 12.5, 1e-9);
    g_assert_true(isnan(p->salt));
    g_assert_true(isnan(p->sugars));
    g_assert_true(isnan(p->protein));
    g_assert_cmpint(p->additives_count, ==, 2);
    g_assert_cmpstr(p->additives[0], ==, "e100");
    g_assert_cmpstr(p->additives[1], ==, "e200");
    product_free(p);
}

static void test_nova_double(void) {
    Product *p = parse("{\"status\":1,\"product\":{\"nova_group\":2.0}}");
    g_assert_nonnull(p);
    g_assert_cmpstr(p->nova_group, ==, "2");
    product_free(p);
    p = parse("{\"status\":1,\"product\":{\"nova_group\":9}}");
    g_assert_nonnull(p);
    g_assert_null(p->nova_group);
    product_free(p);
}

static void test_not_found(void) {
    g_assert_null(parse("{\"status\":0,\"status_verbose\":\"product not found\"}"));
    g_assert_null(parse("{\"status\":1}"));
    g_assert_null(parse("{\"status\":1,\"product\":\"oops\"}"));
    g_assert_null(parse("{\"product\":{\"product_name\":\"x\"}}"));
    g_assert_null(parse("[1,2,3]"));
    g_assert_null(parse_product_json(NULL, "1"));
}

static void test_barcode_normalize(void) {
    char *b;
    b = barcode_normalize("3017620422003");
    g_assert_cmpstr(b, ==, "3017620422003"); g_free(b);
    b = barcode_normalize("  3017 6204-22003 \n");
    g_assert_cmpstr(b, ==, "3017620422003"); g_free(b);
    g_assert_null(barcode_normalize(NULL));
    g_assert_null(barcode_normalize(""));
    g_assert_null(barcode_normalize("   "));
    g_assert_null(barcode_normalize("123"));                 /* too short */
    g_assert_null(barcode_normalize("1234567890123456789012345")); /* too long */
    g_assert_null(barcode_normalize("../../etc/passwd"));
    g_assert_null(barcode_normalize("3017620422003?x=1"));
    g_assert_null(barcode_normalize("30176204%2F22003"));
}

static void test_nutrient_level_nan(void) {
    g_assert_null(nutrient_level("fat", NAN));
    g_assert_cmpstr(nutrient_level("fat", 1.0), ==, "LOW");
    g_assert_null(nutrient_level(NULL, 1.0));
}

int main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/product/full", test_full_product);
    g_test_add_func("/product/null-fields", test_null_fields);
    g_test_add_func("/product/wrong-types", test_wrong_types);
    g_test_add_func("/product/nova-double", test_nova_double);
    g_test_add_func("/product/not-found", test_not_found);
    g_test_add_func("/barcode/normalize", test_barcode_normalize);
    g_test_add_func("/nutrient/level-nan", test_nutrient_level_nan);
    return g_test_run();
}
