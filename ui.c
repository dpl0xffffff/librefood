#include "librefood.h"

/* ────────────────────────────────────────────────
 *  Small widget helpers
 * ──────────────────────────────────────────────── */

static GtkWidget *label_new_styled(const char *text, const char *css_class) {
    GtkWidget *lbl = gtk_label_new(text);
    gtk_widget_add_css_class(lbl, css_class);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    return lbl;
}

GtkWidget *make_nutrient_row(const char *name, double value,
                              const char *unit, const char *level_key) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(row, "ns-nutrient-row");

    GtkWidget *name_lbl = gtk_label_new(name);
    gtk_widget_add_css_class(name_lbl, "ns-nutrient-name");
    gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
    gtk_widget_set_hexpand(name_lbl, TRUE);

    char val_str[64];
    if (isnan(value))                     /* unknown — not the same as 0 */
        snprintf(val_str, sizeof(val_str), "—");
    else
        snprintf(val_str, sizeof(val_str), "%.1f %s", value, unit);

    GtkWidget *val_lbl = gtk_label_new(val_str);
    gtk_widget_add_css_class(val_lbl, "ns-nutrient-value");

    gtk_box_append(GTK_BOX(row), name_lbl);
    gtk_box_append(GTK_BOX(row), val_lbl);

    if (level_key) {
        const char *level = nutrient_level(level_key, value);
        if (level) {
            const char *css = (strcmp(level, "LOW") == 0)  ? "ns-level-low"  :
                              (strcmp(level, "MOD") == 0)  ? "ns-level-mod"  :
                                                              "ns-level-high";
            GtkWidget *lvl_lbl = gtk_label_new(level);
            gtk_widget_add_css_class(lvl_lbl, css);
            gtk_box_append(GTK_BOX(row), lvl_lbl);
        }
    }

    return row;
}

/* ────────────────────────────────────────────────
 *  History list row factory
 * ──────────────────────────────────────────────── */

static void on_history_activated(GtkListBox *lb, GtkListBoxRow *row, gpointer user_data) {
    (void)lb;
    AppState *state = user_data;
    int idx = gtk_list_box_row_get_index(row);
    if (idx < 0) return;
    GList *entry = g_list_nth(state->history, (guint)idx);
    if (!entry) return;
    state->current_product = entry->data;
    show_product(state, entry->data);   /* re-fetches the image */
}

static GtkWidget *make_history_row(Product *p) {
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(row_box, "ns-history-row");

    /* Score badge */
    GtkWidget *badge = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_valign(badge, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(badge, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(badge, "ns-score-badge");
    gtk_widget_add_css_class(badge, score_color(p->nutriscore_grade));
    gtk_widget_set_size_request(badge, 40, 40);

    const char *gl = (p->nutriscore_grade && p->nutriscore_grade[0]) ?
                     p->nutriscore_grade : "?";
    char upper[4] = {0};
    upper[0] = (char)g_ascii_toupper(gl[0]);
    GtkWidget *gl_lbl = gtk_label_new(upper);
    gtk_widget_add_css_class(gl_lbl, "ns-score-letter");
    gtk_widget_set_valign(gl_lbl, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(gl_lbl, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(badge), gl_lbl);

    /* Text */
    GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(text_box, TRUE);
    gtk_widget_set_valign(text_box, GTK_ALIGN_CENTER);

    GtkWidget *name = gtk_label_new(p->product_name);
    gtk_widget_add_css_class(name, "ns-history-name");
    gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);

    GtkWidget *brand = gtk_label_new(p->brand && p->brand[0] ? p->brand : "—");
    gtk_widget_add_css_class(brand, "ns-history-brand");
    gtk_label_set_xalign(GTK_LABEL(brand), 0.0f);

    gtk_box_append(GTK_BOX(text_box), name);
    gtk_box_append(GTK_BOX(text_box), brand);

    GtkWidget *arrow = gtk_label_new("›");
    gtk_widget_add_css_class(arrow, "ns-history-brand");
    gtk_widget_set_valign(arrow, GTK_ALIGN_CENTER);

    gtk_box_append(GTK_BOX(row_box), badge);
    gtk_box_append(GTK_BOX(row_box), text_box);
    gtk_box_append(GTK_BOX(row_box), arrow);

    return row_box;
}

/* Rebuilds the "Recent scans" list; row index == position in history. */
static void refresh_history(AppState *state) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(state->history_list)))
        gtk_list_box_remove(GTK_LIST_BOX(state->history_list), child);

    for (GList *l = state->history; l; l = l->next)
        gtk_list_box_insert(GTK_LIST_BOX(state->history_list),
                            make_history_row(l->data), -1);
}

/* Takes ownership of p. Re-scanning a product moves it to the top instead of
 * duplicating it, and the list is bounded to LF_HISTORY_MAX entries. */
void history_add(AppState *state, Product *p) {
    GList *l = state->history;
    while (l) {
        GList *next = l->next;
        Product *old = l->data;
        if (old != p && g_strcmp0(old->barcode, p->barcode) == 0) {
            if (state->current_product == old) state->current_product = NULL;
            product_free(old);
            state->history = g_list_delete_link(state->history, l);
        }
        l = next;
    }
    state->history = g_list_prepend(state->history, p);

    while (g_list_length(state->history) > LF_HISTORY_MAX) {
        GList *last = g_list_last(state->history);
        if (state->current_product == last->data) state->current_product = NULL;
        product_free(last->data);
        state->history = g_list_delete_link(state->history, last);
    }
    state->current_product = p;
    refresh_history(state);
}

/* ────────────────────────────────────────────────
 *  Home page
 * ──────────────────────────────────────────────── */

static void on_scan_clicked(GtkButton *btn, gpointer user_data);
static void on_scan_entry_activate(GtkEntry *entry, gpointer user_data);
static void on_camera_scan_clicked(GtkButton *btn, gpointer user_data);

static void on_example_clicked(GtkButton *btn, gpointer user_data) {
    const char *bc = g_object_get_data(G_OBJECT(btn), "barcode");
    if (bc) do_lookup(user_data, bc);
}

/* Stops the scanner subprocess and any pending read. Safe to call repeatedly. */
static void stop_camera_scan(AppState *state) {
    if (state->camera_cancel) {
        g_cancellable_cancel(state->camera_cancel);
        g_clear_object(&state->camera_cancel);
    }
    g_clear_object(&state->camera_stream);
    if (state->camera_proc) {
        /* force_exit is a no-op once the child has exited (and GSubprocess
         * reaps it), so no exit check is needed — calling
         * g_subprocess_get_if_exited() on a running child is a critical. */
        g_subprocess_force_exit(state->camera_proc);
        g_clear_object(&state->camera_proc);
    }
}

static void set_camera_status(AppState *state, const char *text) {
    if (state->camera_status_label)
        gtk_label_set_text(GTK_LABEL(state->camera_status_label), text);
}

/* Returns the first run of >= 8 digits in text (EAN-8 and longer). */
static char *extract_barcode(const char *text) {
    if (!text) return NULL;
    const char *p = text;
    while (*p) {
        while (*p && !g_ascii_isdigit(*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && g_ascii_isdigit(*p)) p++;
        gsize len = (gsize)(p - start);
        if (len >= 8 && len <= LF_BARCODE_MAX_LEN) return g_strndup(start, len);
    }
    return NULL;
}

static void start_camera_read(AppState *state);

/* The scanner prints exactly one line on stdout: either the barcode or a
 * line starting with "ERROR:". Diagnostics go to stderr (inherited). */
static void on_camera_line_read(GObject *source, GAsyncResult *res, gpointer user_data) {
    AppState *state = user_data;
    GError *err = NULL;
    gsize len = 0;
    char *line = g_data_input_stream_read_line_finish_utf8(
        G_DATA_INPUT_STREAM(source), res, &len, &err);

    /* Cancelled, or a stale read from a previous scan session: the source
     * stream is no longer ours, so don't touch scan state. */
    if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
        (GObject *)state->camera_stream != source) {
        g_clear_error(&err);
        g_free(line);
        return;
    }

    if (err) {
        g_warning("librefood: reading scanner output failed: %s", err->message);
        set_camera_status(state, "Camera scan stopped unexpectedly.");
        g_clear_error(&err);
        stop_camera_scan(state);
        return;
    }

    if (!line) {                                   /* EOF: scanner exited */
        set_camera_status(state,
            "Scanner closed without reading a barcode. "
            "Close this window and try again.");
        stop_camera_scan(state);
        return;
    }

    g_strstrip(line);
    if (g_str_has_prefix(line, "ERROR:")) {
        set_camera_status(state, line);
        g_free(line);
        stop_camera_scan(state);
        return;
    }

    char *barcode = extract_barcode(line);
    g_free(line);
    if (!barcode) {                                /* ignore stray output */
        start_camera_read(state);
        return;
    }

    g_print("Scanned barcode: %s\n", barcode);
    /* Destroying the dialog runs on_camera_dialog_destroy → stop_camera_scan. */
    if (state->camera_dialog)
        gtk_window_destroy(GTK_WINDOW(state->camera_dialog));
    stop_camera_scan(state);
    do_lookup(state, barcode);
    g_free(barcode);
}

static void start_camera_read(AppState *state) {
    if (!state->camera_stream || !state->camera_cancel) return;
    g_data_input_stream_read_line_async(
        state->camera_stream, G_PRIORITY_DEFAULT, state->camera_cancel,
        on_camera_line_read, state);
}

/* "destroy" fires for every way the dialog goes away (window close button,
 * Cancel button, successful scan, parent window destroyed). */
static void on_camera_dialog_destroy(GtkWidget *win, gpointer user_data) {
    (void)win;
    AppState *state = user_data;
    stop_camera_scan(state);
    state->camera_dialog = NULL;
    state->camera_status_label = NULL;
}

/* Directory containing the running executable, or NULL. */
static char *exe_dir(void) {
    char *exe = g_file_read_link("/proc/self/exe", NULL);
    if (!exe) return NULL;
    char *dir = g_path_get_dirname(exe);
    g_free(exe);
    return dir;
}

/* Finds opencv_scanner.py and a Python interpreter for it. Looks next to the
 * executable first, then in the current directory. Prefers a project .venv
 * and falls back to python3 on PATH (system-wide py3-opencv/py3-pyzbar). */
static gboolean find_scanner(char **python_out, char **script_out) {
    char *dirs[2] = { exe_dir(), g_get_current_dir() };
    gboolean found = FALSE;
    for (int i = 0; i < 2 && !found; i++) {
        if (!dirs[i]) continue;
        char *script = g_build_filename(dirs[i], "opencv_scanner.py", NULL);
        if (g_file_test(script, G_FILE_TEST_IS_REGULAR)) {
            char *venv = g_build_filename(dirs[i], ".venv", "bin", "python3", NULL);
            if (g_file_test(venv, G_FILE_TEST_IS_EXECUTABLE)) {
                *python_out = venv;
            } else {
                g_free(venv);
                *python_out = g_find_program_in_path("python3");
            }
            if (*python_out) {
                *script_out = script;
                found = TRUE;
                continue;
            }
        }
        g_free(script);
    }
    g_free(dirs[0]);
    g_free(dirs[1]);
    return found;
}

GtkWidget *build_home_page(AppState *state) {
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), vbox);

    /* ── Hero card ── */
    GtkWidget *hero = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(hero, "ns-hero");

    GtkWidget *hero_title = gtk_label_new("librefood");
    gtk_widget_add_css_class(hero_title, "ns-hero-title");
    gtk_label_set_xalign(GTK_LABEL(hero_title), 0.0f);

    GtkWidget *hero_sub = gtk_label_new(
        "Enter a barcode to check the nutritional quality of any food product.");
    gtk_widget_add_css_class(hero_sub, "ns-hero-sub");
    gtk_label_set_xalign(GTK_LABEL(hero_sub), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(hero_sub), TRUE);

    /* Scan input row */
    GtkWidget *scan_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(scan_row, "ns-scan-box");

    state->scan_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->scan_entry),
        "Barcode  e.g. 3017620422003");
    gtk_widget_add_css_class(state->scan_entry, "ns-scan-entry");
    gtk_widget_set_hexpand(state->scan_entry, TRUE);
    g_signal_connect(state->scan_entry, "activate",
                     G_CALLBACK(on_scan_entry_activate), state);

    state->scan_button = gtk_button_new_with_label("Search");
    gtk_widget_add_css_class(state->scan_button, "ns-scan-btn");
    g_signal_connect(state->scan_button, "clicked",
                     G_CALLBACK(on_scan_clicked), state);

    state->camera_button = gtk_button_new_with_label("Scan with Camera");
    gtk_widget_add_css_class(state->camera_button, "ns-scan-btn");
    g_signal_connect(state->camera_button, "clicked",
                     G_CALLBACK(on_camera_scan_clicked), state);

    gtk_box_append(GTK_BOX(scan_row), state->scan_entry);
    gtk_box_append(GTK_BOX(scan_row), state->scan_button);
    gtk_box_append(GTK_BOX(scan_row), state->camera_button);

    gtk_box_append(GTK_BOX(hero), hero_title);
    gtk_box_append(GTK_BOX(hero), hero_sub);
    gtk_box_append(GTK_BOX(hero), scan_row);

    /* ── Quick-tip pills ── */
    GtkWidget *tips = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(tips, 12);
    const char *examples[] = {
        "Nutella  3017620422003",
        "Coca-Cola  5449000000996",
        "Evian  3068320123903",
        NULL
    };
    for (int i = 0; examples[i]; i++) {
        const char *parts = strrchr(examples[i], ' ');
        if (!parts) continue;
        char *bc = g_strdup(parts + 1);
        GtkWidget *pill = gtk_button_new_with_label(bc);
        gtk_widget_add_css_class(pill, "ns-scan-btn");
        g_object_set_data_full(G_OBJECT(pill), "barcode", bc, g_free);
        g_signal_connect(pill, "clicked", G_CALLBACK(on_example_clicked), state);
        gtk_box_append(GTK_BOX(tips), pill);
    }
    gtk_box_append(GTK_BOX(hero), tips);

    gtk_box_append(GTK_BOX(vbox), hero);

    /* ── History section ── */
    GtkWidget *hist_title = gtk_label_new("RECENT SCANS");
    gtk_widget_add_css_class(hist_title, "ns-section-title");
    gtk_widget_set_margin_top(hist_title, 20);
    gtk_widget_set_margin_bottom(hist_title, 8);
    gtk_box_append(GTK_BOX(vbox), hist_title);

    state->history_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(state->history_list),
        GTK_SELECTION_NONE);
    gtk_widget_add_css_class(state->history_list, "boxed-list");
    g_signal_connect(state->history_list, "row-activated",
                     G_CALLBACK(on_history_activated), state);

    /* Empty-state placeholder */
    GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(empty_box, 32);
    gtk_widget_set_margin_bottom(empty_box, 32);

    GtkWidget *empty_icon = gtk_label_new("🔍");
    gtk_widget_add_css_class(empty_icon, "ns-empty-icon");
    GtkWidget *empty_text = gtk_label_new("No products scanned yet.");
    gtk_widget_add_css_class(empty_text, "ns-empty-text");

    gtk_box_append(GTK_BOX(empty_box), empty_icon);
    gtk_box_append(GTK_BOX(empty_box), empty_text);
    gtk_list_box_set_placeholder(GTK_LIST_BOX(state->history_list), empty_box);

    gtk_box_append(GTK_BOX(vbox), state->history_list);

    return scroll;
}

/* ────────────────────────────────────────────────
 *  Loading page
 * ──────────────────────────────────────────────── */

GtkWidget *build_loading_page(AppState *state) {
    GtkWidget *center = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_halign(center, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(center, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(center, TRUE);

    state->spinner = gtk_spinner_new();
    gtk_widget_set_size_request(state->spinner, 48, 48);
    gtk_spinner_start(GTK_SPINNER(state->spinner));

    state->loading_label = gtk_label_new("Looking up product…");
    gtk_widget_add_css_class(state->loading_label, "ns-loading-label");

    gtk_box_append(GTK_BOX(center), state->spinner);
    gtk_box_append(GTK_BOX(center), state->loading_label);

    return center;
}

/* ────────────────────────────────────────────────
 *  Product page
 * ──────────────────────────────────────────────── */

GtkWidget *build_product_page(AppState *state) {
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), vbox);

    /* ── Product image ── */
    state->product_image = gtk_image_new_from_icon_name("image-x-generic");
    gtk_image_set_pixel_size(GTK_IMAGE(state->product_image), 200);
    gtk_widget_set_halign(state->product_image, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(state->product_image, 20);
    gtk_widget_add_css_class(state->product_image, "ns-product-image");
    gtk_box_append(GTK_BOX(vbox), state->product_image);

    /* ── Name + brand ── */
    GtkWidget *meta_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(meta_box, 20);
    gtk_widget_set_margin_end(meta_box, 20);
    gtk_widget_set_margin_top(meta_box, 12);

    state->brand_label = label_new_styled("", "ns-product-brand");
    state->product_name_label = label_new_styled("", "ns-product-name");

    gtk_box_append(GTK_BOX(meta_box), state->brand_label);
    gtk_box_append(GTK_BOX(meta_box), state->product_name_label);
    gtk_box_append(GTK_BOX(vbox), meta_box);

    /* ── Score + NOVA row ── */
    GtkWidget *score_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_start(score_row, 20);
    gtk_widget_set_margin_end(score_row, 20);
    gtk_widget_set_margin_top(score_row, 16);

    /* Nutriscore */
    GtkWidget *ns_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    GtkWidget *ns_header = gtk_label_new("NUTRI-SCORE");
    gtk_widget_add_css_class(ns_header, "ns-section-title");
    gtk_label_set_xalign(GTK_LABEL(ns_header), 0.0f);
    gtk_widget_set_margin_start(ns_header, 0);
    gtk_widget_set_margin_end(ns_header, 0);

    state->score_badge = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(state->score_badge, "ns-score-badge");
    gtk_widget_set_halign(state->score_badge, GTK_ALIGN_START);
    gtk_widget_set_size_request(state->score_badge, 60, 60);

    state->score_letter = gtk_label_new("?");
    gtk_widget_add_css_class(state->score_letter, "ns-score-letter");
    gtk_widget_set_halign(state->score_letter, GTK_ALIGN_CENTER);

    state->score_desc = gtk_label_new("");
    gtk_widget_add_css_class(state->score_desc, "ns-score-word");
    gtk_widget_set_halign(state->score_desc, GTK_ALIGN_CENTER);

    gtk_box_append(GTK_BOX(state->score_badge), state->score_letter);
    gtk_box_append(GTK_BOX(state->score_badge), state->score_desc);

    gtk_box_append(GTK_BOX(ns_box), ns_header);
    gtk_box_append(GTK_BOX(ns_box), state->score_badge);

    /* NOVA */
    GtkWidget *nova_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    GtkWidget *nova_header = gtk_label_new("NOVA GROUP");
    gtk_widget_add_css_class(nova_header, "ns-section-title");
    gtk_label_set_xalign(GTK_LABEL(nova_header), 0.0f);
    gtk_widget_set_margin_start(nova_header, 0);
    gtk_widget_set_margin_end(nova_header, 0);

    state->nova_badge = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(state->nova_badge, "ns-nova-badge");
    gtk_widget_set_halign(state->nova_badge, GTK_ALIGN_START);
    gtk_widget_set_size_request(state->nova_badge, 60, 60);

    GtkWidget *nova_num = gtk_label_new("?");
    gtk_widget_add_css_class(nova_num, "ns-nova-num");
    gtk_widget_set_halign(nova_num, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(nova_num, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(state->nova_badge), nova_num);

    state->nova_desc = gtk_label_new("");
    gtk_widget_add_css_class(state->nova_desc, "ns-score-word");
    gtk_label_set_xalign(GTK_LABEL(state->nova_desc), 0.0f);

    gtk_box_append(GTK_BOX(nova_box), nova_header);
    gtk_box_append(GTK_BOX(nova_box), state->nova_badge);
    gtk_box_append(GTK_BOX(nova_box), state->nova_desc);

    gtk_box_append(GTK_BOX(score_row), ns_box);
    gtk_box_append(GTK_BOX(score_row), nova_box);
    gtk_widget_set_hexpand(nova_box, TRUE);
    gtk_box_append(GTK_BOX(vbox), score_row);

    /* ── Nutrition card ── */
    GtkWidget *nut_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(nut_card, "ns-card");
    gtk_widget_set_margin_top(nut_card, 16);

    GtkWidget *nut_title = gtk_label_new("Nutrition  per 100 g");
    gtk_widget_add_css_class(nut_title, "ns-card-title");
    gtk_label_set_xalign(GTK_LABEL(nut_title), 0.0f);
    gtk_box_append(GTK_BOX(nut_card), nut_title);

    state->nutrition_grid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(nut_card), state->nutrition_grid);
    gtk_box_append(GTK_BOX(vbox), nut_card);

    /* ── Additives card ── */
    GtkWidget *add_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(add_card, "ns-card");
    gtk_widget_set_margin_top(add_card, 8);

    GtkWidget *add_title = gtk_label_new("Additives");
    gtk_widget_add_css_class(add_title, "ns-card-title");
    gtk_label_set_xalign(GTK_LABEL(add_title), 0.0f);
    gtk_box_append(GTK_BOX(add_card), add_title);

    state->additives_box = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(state->additives_box),
        GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(state->additives_box), FALSE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(state->additives_box), 10);
    gtk_box_append(GTK_BOX(add_card), state->additives_box);
    gtk_box_append(GTK_BOX(vbox), add_card);

    /* ── Ingredients card ── */
    GtkWidget *ing_card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(ing_card, "ns-card");
    gtk_widget_set_margin_top(ing_card, 8);
    gtk_widget_set_margin_bottom(ing_card, 24);

    GtkWidget *ing_title = gtk_label_new("Ingredients");
    gtk_widget_add_css_class(ing_title, "ns-card-title");
    gtk_label_set_xalign(GTK_LABEL(ing_title), 0.0f);

    state->ingredients_label = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(state->ingredients_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(state->ingredients_label), 0.0f);
    gtk_widget_add_css_class(state->ingredients_label, "ns-nutrient-name");

    gtk_box_append(GTK_BOX(ing_card), ing_title);
    gtk_box_append(GTK_BOX(ing_card), state->ingredients_label);
    gtk_box_append(GTK_BOX(vbox), ing_card);

    return scroll;
}

/* ────────────────────────────────────────────────
 *  Navigation
 * ──────────────────────────────────────────────── */

static void on_back_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    show_home(user_data);
}

void show_home(AppState *state) {
    gtk_spinner_stop(GTK_SPINNER(state->spinner));
    gtk_label_set_text(GTK_LABEL(state->title_label), "librefood");
    gtk_widget_set_visible(state->back_button, FALSE);
    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "home");
}

void show_error(AppState *state, const char *msg) {
    const char *text = msg ? msg : "Product not found. Check the barcode and try again.";
    GtkAlertDialog *dialog = gtk_alert_dialog_new("%s", "Lookup error");
    gtk_alert_dialog_set_detail(dialog, text);
    gtk_alert_dialog_show(dialog, GTK_WINDOW(state->window));
    g_object_unref(dialog);
    show_home(state);
}

void show_product(AppState *state, Product *p) {
    if (!p) {
        show_error(state, "Product not found in the OpenFoodFacts database.");
        return;
    }

    /* ── Meta ── */
    gtk_label_set_text(GTK_LABEL(state->product_name_label), p->product_name);
    gtk_label_set_text(GTK_LABEL(state->brand_label),
        (p->brand && p->brand[0]) ? p->brand : "Unknown brand");

    /* ── Nutriscore ── */
    /* Clear old CSS classes */
    const char *grades[] = {"ns-score-a","ns-score-b","ns-score-c",
                             "ns-score-d","ns-score-e","ns-score-unknown", NULL};
    for (int i = 0; grades[i]; i++)
        gtk_widget_remove_css_class(state->score_badge, grades[i]);

    gtk_widget_add_css_class(state->score_badge, score_color(p->nutriscore_grade));

    char upper[4] = {0};
    if (p->nutriscore_grade && p->nutriscore_grade[0])
        upper[0] = (char)g_ascii_toupper(p->nutriscore_grade[0]);
    else
        upper[0] = '?';

    gtk_label_set_text(GTK_LABEL(state->score_letter), upper);
    gtk_label_set_text(GTK_LABEL(state->score_desc),
        score_description(p->nutriscore_grade));

    /* ── NOVA ── */
    const char *nova_classes[] = {
        "ns-nova-1","ns-nova-2","ns-nova-3","ns-nova-4","ns-nova-unknown", NULL};
    for (int i = 0; nova_classes[i]; i++)
        gtk_widget_remove_css_class(state->nova_badge, nova_classes[i]);

    char nova_css[16] = "ns-nova-unknown";
    if (p->nova_group && p->nova_group[0] >= '1' && p->nova_group[0] <= '4') {
        snprintf(nova_css, sizeof(nova_css), "ns-nova-%c", p->nova_group[0]);
    }
    gtk_widget_add_css_class(state->nova_badge, nova_css);

    /* Update nova number label (first child of nova_badge box) */
    GtkWidget *nova_num = gtk_widget_get_first_child(state->nova_badge);
    if (nova_num && GTK_IS_LABEL(nova_num)) {
        gtk_label_set_text(GTK_LABEL(nova_num),
            (p->nova_group && p->nova_group[0]) ? p->nova_group : "?");
    }
    gtk_label_set_text(GTK_LABEL(state->nova_desc), nova_description(p->nova_group));

    /* ── Nutrition grid ── */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(state->nutrition_grid)))
        gtk_box_remove(GTK_BOX(state->nutrition_grid), child);

    struct { const char *name; double val; const char *unit; const char *key; } rows[] = {
        { "Energy",           p->energy_kcal,   "kcal", NULL           },
        { "Fat",              p->fat,            "g",    "fat"          },
        { "  Saturated fat",  p->saturated_fat,  "g",    "saturated_fat"},
        { "Carbohydrates",    p->carbs,          "g",    NULL           },
        { "  Sugars",         p->sugars,         "g",    "sugars"       },
        { "Dietary fiber",    p->fiber,          "g",    NULL           },
        { "Protein",          p->protein,        "g",    NULL           },
        { "Salt",             p->salt,           "g",    "salt"         },
        { NULL, 0, NULL, NULL }
    };
    for (int i = 0; rows[i].name; i++) {
        GtkWidget *row = make_nutrient_row(
            rows[i].name, rows[i].val, rows[i].unit, rows[i].key);
        gtk_box_append(GTK_BOX(state->nutrition_grid), row);
    }

    /* ── Additives ── */
    while ((child = gtk_widget_get_first_child(state->additives_box)))
        gtk_flow_box_remove(GTK_FLOW_BOX(state->additives_box), child);

    if (p->additives_count == 0) {
        GtkWidget *none = gtk_label_new("No additives listed ✓");
        gtk_widget_add_css_class(none, "ns-nutrient-name");
        gtk_flow_box_insert(GTK_FLOW_BOX(state->additives_box), none, -1);
    } else {
        for (int i = 0; i < p->additives_count; i++) {
            GtkWidget *chip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_add_css_class(chip, "ns-additive-chip");
            GtkWidget *lbl = gtk_label_new(p->additives[i]);
            gtk_widget_add_css_class(lbl, "ns-additive-label");
            gtk_box_append(GTK_BOX(chip), lbl);
            gtk_flow_box_insert(GTK_FLOW_BOX(state->additives_box), chip, -1);
        }
    }

    /* ── Ingredients ── */
    gtk_label_set_text(GTK_LABEL(state->ingredients_label),
        (p->ingredients_text && p->ingredients_text[0]) ?
        p->ingredients_text : "Not available.");

    /* ── Reset product image, then (re)fetch it; cancels any older one ── */
    gtk_image_set_from_icon_name(GTK_IMAGE(state->product_image),
        "image-x-generic");
    fetch_image(state, p->image_url);

    /* ── Switch to product page ── */
    /* Truncate by characters, not bytes: a byte cut can split a UTF-8
     * sequence and produce an invalid label string. */
    char *title = g_utf8_strlen(p->product_name, -1) > 40
        ? g_utf8_substring(p->product_name, 0, 40)
        : g_strdup(p->product_name);
    gtk_label_set_text(GTK_LABEL(state->title_label), title);
    g_free(title);
    gtk_spinner_stop(GTK_SPINNER(state->spinner));
    gtk_widget_set_visible(state->back_button, TRUE);
    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "product");
}

/* ────────────────────────────────────────────────
 *  Scan action
 * ──────────────────────────────────────────────── */

gboolean do_lookup(AppState *state, const char *input) {
    char *barcode = barcode_normalize(input);
    if (!barcode) {
        show_error(state, "Please enter a valid barcode (digits only, "
                          "e.g. 3017620422003).");
        return FALSE;
    }

    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "loading");
    gtk_spinner_start(GTK_SPINNER(state->spinner));
    gtk_widget_set_visible(state->back_button, FALSE);
    gtk_label_set_text(GTK_LABEL(state->title_label), "Searching…");

    fetch_product(state, barcode);
    g_free(barcode);
    return TRUE;
}

static void on_scan_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *state = user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(state->scan_entry));
    if (!text || text[0] == '\0') return;
    /* Keep the text on invalid input so the user can correct it. */
    if (do_lookup(state, text))
        gtk_editable_set_text(GTK_EDITABLE(state->scan_entry), "");
}

static void on_scan_entry_activate(GtkEntry *entry, gpointer user_data) {
    (void)entry;
    on_scan_clicked(NULL, user_data);
}

static void on_camera_scan_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *state = user_data;

    if (state->camera_dialog) {
        gtk_window_present(GTK_WINDOW(state->camera_dialog));
        return;
    }

    char *python = NULL, *script = NULL;
    if (!find_scanner(&python, &script)) {
        show_error(state, "Camera scanner unavailable: opencv_scanner.py or "
                          "python3 not found. Run build.sh to install it.");
        return;
    }

    GtkWidget *dialog = gtk_window_new();
    state->camera_dialog = dialog;
    gtk_window_set_title(GTK_WINDOW(dialog), "Scan Barcode");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(state->window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 380, 160);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_widget_set_margin_start(box, 16);
    gtk_widget_set_margin_end(box, 16);

    GtkWidget *title = gtk_label_new("OpenCV scanner preview opens in separate window.");
    gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
    gtk_widget_add_css_class(title, "ns-card-title");

    state->camera_status_label = gtk_label_new(
        "Align barcode in preview. Scan is automatic; press Esc to cancel.");
    gtk_label_set_wrap(GTK_LABEL(state->camera_status_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(state->camera_status_label), 0.0f);

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel, "ns-back-btn");
    g_signal_connect_swapped(cancel, "clicked", G_CALLBACK(gtk_window_destroy), dialog);

    gtk_box_append(GTK_BOX(box), title);
    gtk_box_append(GTK_BOX(box), state->camera_status_label);
    gtk_box_append(GTK_BOX(box), cancel);
    gtk_window_set_child(GTK_WINDOW(dialog), box);

    g_signal_connect(dialog, "destroy", G_CALLBACK(on_camera_dialog_destroy), state);

    const char *argv[] = { python, script, NULL };
    GError *err = NULL;
    /* stderr is inherited (not merged) so warnings/tracebacks can't be
     * mistaken for a barcode and remain visible for debugging. */
    state->camera_proc = g_subprocess_newv(argv, G_SUBPROCESS_FLAGS_STDOUT_PIPE, &err);
    g_free(python);
    g_free(script);
    if (!state->camera_proc) {
        gtk_label_set_text(GTK_LABEL(state->camera_status_label),
                           err ? err->message : "Unable to access camera.");
        g_clear_error(&err);
        gtk_widget_set_visible(dialog, TRUE);
        return;
    }

    state->camera_stream = g_data_input_stream_new(
        g_subprocess_get_stdout_pipe(state->camera_proc));
    state->camera_cancel = g_cancellable_new();
    start_camera_read(state);
    gtk_widget_set_visible(dialog, TRUE);
}

/* ────────────────────────────────────────────────
 *  App activation
 * ──────────────────────────────────────────────── */

/* Main window closing: kill the scanner child (it would otherwise outlive
 * the app) and cancel network I/O. AppState itself is intentionally not
 * freed: cancelled async callbacks may still be dispatched and reference it,
 * and the process exits right after. */
static void on_main_window_destroy(GtkWidget *win, gpointer user_data) {
    (void)win;
    AppState *state = user_data;
    if (state->camera_dialog)
        gtk_window_destroy(GTK_WINDOW(state->camera_dialog));
    stop_camera_scan(state);
    cancel_network(state);
}

void on_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    /* A second launch re-activates the running instance: reuse its window
     * instead of building a second, independent AppState. */
    GtkWindow *existing = gtk_application_get_active_window(app);
    if (existing) {
        gtk_window_present(existing);
        return;
    }

    apply_css();

    AppState *state = g_new0(AppState, 1);
    state->app = app;
    g_object_set_data(G_OBJECT(app), "librefood-state", state);
    state->http_session = soup_session_new_with_options(
        "user-agent", LF_USER_AGENT,          /* OFF requires a UA */
        "timeout", LF_HTTP_TIMEOUT_SEC,       /* never hang on the spinner */
        NULL);

    /* Window */
    state->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(state->window), "librefood");
    gtk_window_set_default_size(GTK_WINDOW(state->window), 420, 700);
    gtk_window_set_resizable(GTK_WINDOW(state->window), TRUE);
    g_signal_connect(state->window, "destroy", G_CALLBACK(on_main_window_destroy), state);

    /* Header bar */
    state->header_bar = gtk_header_bar_new();
    gtk_widget_add_css_class(state->header_bar, "ns-header");

    state->back_button = gtk_button_new_with_label("← Back");
    gtk_widget_add_css_class(state->back_button, "ns-back-btn");
    gtk_widget_set_visible(state->back_button, FALSE);
    g_signal_connect(state->back_button, "clicked",
                     G_CALLBACK(on_back_clicked), state);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(state->header_bar),
                              state->back_button);

    state->title_label = gtk_label_new("librefood");
    gtk_widget_add_css_class(state->title_label, "ns-title");
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(state->header_bar),
                                    state->title_label);

    gtk_window_set_titlebar(GTK_WINDOW(state->window), state->header_bar);

    /* Stack */
    state->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(state->stack),
        GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(state->stack), 250);

    gtk_stack_add_named(GTK_STACK(state->stack),
        build_home_page(state), "home");
    gtk_stack_add_named(GTK_STACK(state->stack),
        build_loading_page(state), "loading");
    gtk_stack_add_named(GTK_STACK(state->stack),
        build_product_page(state), "product");

    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "home");
    gtk_window_set_child(GTK_WINDOW(state->window), state->stack);
    gtk_window_present(GTK_WINDOW(state->window));
}
