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

/* ── Data model ── */
typedef struct {
    char *barcode;
    char *product_name;
    char *brand;
    char *image_url;
    char *nutriscore_grade;   /* "a".."e" or NULL */
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
    char **additives;         /* NULL-terminated list */
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
    GtkWidget       *history_list;

    /* ── Loading page ── */
    GtkWidget       *loading_page;
    GtkWidget       *spinner;
    GtkWidget       *loading_label;

    /* ── Product page ── */
    GtkWidget       *product_page;
    GtkWidget       *product_image;
    GtkWidget       *product_name_label;
    GtkWidget       *brand_label;
    GtkWidget       *score_badge;
    GtkWidget       *score_letter;
    GtkWidget       *score_desc;
    GtkWidget       *nova_badge;
    GtkWidget       *nutrition_grid;
    GtkWidget       *ingredients_label;
    GtkWidget       *additives_box;

    /* HTTP session */
    SoupSession     *http_session;

    /* Current product */
    Product         *current_product;

    /* Scan history (GList of Product*) */
    GList           *history;

    /* Camera scan session */
    GtkWidget       *camera_dialog;
    GtkWidget       *camera_status_label;
    GSubprocess     *camera_preview_proc;
    GSubprocess     *camera_proc;
    GInputStream    *camera_stream;
    GCancellable    *camera_cancel;
    GString         *camera_buffer;
} AppState;

/* ── Function declarations ── */
void on_activate(GtkApplication *app, gpointer user_data);

/* UI builders */
GtkWidget *build_home_page(AppState *state);
GtkWidget *build_loading_page(AppState *state);
GtkWidget *build_product_page(AppState *state);

/* Actions */
void do_lookup(AppState *state, const char *barcode);
void show_product(AppState *state, Product *p);
void show_home(AppState *state);
void show_error(AppState *state, const char *msg);

/* HTTP / parsing */
void fetch_product(AppState *state, const char *barcode);
Product *parse_product_json(JsonObject *root, const char *barcode);

/* Helpers */
const char *score_color(const char *grade);
const char *score_description(const char *grade);
const char *nova_description(const char *nova);
void product_free(Product *p);
void apply_css(void);
GtkWidget *make_nutrient_row(const char *name, double value, const char *unit, const char *level);
const char *nutrient_level(const char *nutrient, double per100g);
