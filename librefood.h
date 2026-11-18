#pragma once

#include <gtk/gtk.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* ── Colour palette ── */
#define COLOR_BG          "#F5F2EE"
#define COLOR_SURFACE     "#FFFFFF"
#define COLOR_TEXT        "#1A1A1A"
#define COLOR_MUTED       "#7A7A7A"
#define COLOR_ACCENT      "#2ECC71"

#define COLOR_SCORE_A     "#2ECC71"   /* green  – excellent */
#define COLOR_SCORE_B     "#82D047"   /* lime   – good      */
#define COLOR_SCORE_C     "#F1C40F"   /* yellow – moderate  */
#define COLOR_SCORE_D     "#E67E22"   /* orange – poor      */
#define COLOR_SCORE_E     "#E74C3C"   /* red    – bad       */

/* ── Network ── */
#define LF_USER_AGENT          "librefood/1.0 (davide.plozner@etik.com)"
#define LF_HTTP_TIMEOUT_SEC    15     /* per-request I/O timeout */
#define LF_HISTORY_MAX         50     /* bound on in-memory scan history */
#define LF_BARCODE_MIN_LEN     4
#define LF_BARCODE_MAX_LEN     24

/* ── Data model ── */
typedef struct {
    char *barcode;
    char *product_name;
    char *brand;
    char *image_url;
    char *quantity;           /* e.g. "400 g"; "" if unknown */
    char *nutriscore_grade;   /* "a".."e" or NULL */
    char *ecoscore_grade;     /* "a-plus","a".."f" or NULL */
    /* Nutriment values per 100 g; NAN when unknown (distinct from 0). */
    double energy_kcal;
    double fat;
    double saturated_fat;
    double carbs;
    double sugars;
    double fiber;
    double protein;
    double salt;
    char  *ingredients_text;
    char  *nova_group;        /* "1".."4" */
    int    additives_count;
    char **additives;         /* NULL-terminated codes, e.g. "e322" */
    char **allergens;         /* NULL-terminated display names */
    char **traces;            /* NULL-terminated display names */
    char **labels;            /* NULL-terminated display names */
    GdkTexture *image;        /* downloaded front image, or NULL */
} Product;

/* ── App state ── */
typedef struct {
    GtkApplication  *app;
    GtkWidget       *window;

    /* Header / toolbar */
    GtkWidget       *header_bar;
    GtkWidget       *back_button;
    GtkWidget       *title_label;

    /* Stack pages */
    GtkWidget       *stack;

    /* ── Home page ── */
    GtkWidget       *home_page;
    GtkWidget       *scan_entry;
    GtkWidget       *scan_button;
    GtkWidget       *camera_button;
    GtkWidget       *error_revealer;
    GtkWidget       *error_label;
    GtkWidget       *history_list;
    GtkWidget       *clear_history_button;

    /* ── Loading page ── */
    GtkWidget       *loading_page;
    GtkWidget       *spinner;
    GtkWidget       *loading_label;

    /* ── Product page ── */
    GtkWidget       *product_page;
    GtkWidget       *product_image;
    GtkWidget       *product_name_label;
    GtkWidget       *brand_label;
    GtkWidget       *eco_badge;
    GtkWidget       *eco_letter;
    GtkWidget       *eco_desc;
    GtkWidget       *nova_num;
    GtkWidget       *score_badge;
    GtkWidget       *score_letter;
    GtkWidget       *score_desc;
    GtkWidget       *nova_badge;
    GtkWidget       *nova_desc;
    GtkWidget       *nutrition_grid;
    GtkWidget       *ingredients_label;
    GtkWidget       *additives_box;
    GtkWidget       *allergens_box;
    GtkWidget       *traces_label;
    GtkWidget       *labels_card;
    GtkWidget       *labels_box;
    GtkWidget       *off_link;

    /* HTTP session */
    SoupSession     *http_session;
    GCancellable    *fetch_cancel;   /* in-flight product lookup, or NULL */
    GCancellable    *image_cancel;   /* in-flight image download, or NULL */

    /* Current product (borrowed; owned by history) */
    Product         *current_product;

    /* Scan history (GList of Product*, owns its elements, newest first) */
    GList           *history;

    /* Camera scan session */
    GtkWidget       *camera_dialog;
    GtkWidget       *camera_status_label;
    GSubprocess     *camera_proc;
    GDataInputStream *camera_stream;
    GCancellable    *camera_cancel;
} AppState;

/* ── Function declarations ── */
void on_activate(GtkApplication *app, gpointer user_data);

/* UI builders */
GtkWidget *build_home_page(AppState *state);
GtkWidget *build_loading_page(AppState *state);
GtkWidget *build_product_page(AppState *state);

/* Actions */
/* Validates input; returns FALSE (after showing an error) if invalid. */
gboolean do_lookup(AppState *state, const char *input);
void show_product(AppState *state, Product *p);
void show_home(AppState *state);
void show_error(AppState *state, const char *msg);

/* HTTP / parsing */
void fetch_product(AppState *state, const char *barcode);
/* Shows p's front image: from p->image if cached, else downloads it. */
void fetch_image(AppState *state, Product *p);
void cancel_network(AppState *state);
/* Parses a full OFF API response object. Returns NULL unless status == 1
 * and a product object is present. Tolerates null/mistyped fields. */
Product *parse_product_json(JsonObject *root, const char *barcode);
/* Strips spaces/dashes; returns a newly allocated digits-only barcode, or
 * NULL if the input is not a plausible barcode. */
char *barcode_normalize(const char *input);

/* History */
void history_add(AppState *state, Product *p);
void history_clear(AppState *state);
void refresh_history(AppState *state);
Product *history_find(AppState *state, const char *barcode);

/* Helpers */
const char *score_color(const char *grade);
const char *score_description(const char *grade);
const char *ecoscore_color(const char *grade);
const char *ecoscore_label(const char *grade);   /* "A+", "A".."F", "?" */
const char *ecoscore_description(const char *grade);
/* "en:no-gluten" → "No gluten" (newly allocated) */
char *tag_display_name(const char *tag);
/* "e322i" → "Lecithin"; NULL if unknown */
const char *additive_name(const char *code);
/* "e322i" → "E322i · Lecithin" (newly allocated) */
char *additive_display(const char *code);
/* 539 → "539", 0.107 → "0.11", 30.9 → "30.9" (newly allocated) */
char *format_amount(double v);
double nutrient_fraction(const char *nutrient, double per100g);
const char *nova_description(const char *nova);
void product_free(Product *p);
void apply_css(void);
GtkWidget *make_nutrient_row(const char *name, double value, const char *unit, const char *level);
const char *nutrient_level(const char *nutrient, double per100g);
