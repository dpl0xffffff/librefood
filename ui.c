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

static GtkWidget *chip_new(const char *text, const char *variant) {
    GtkWidget *lbl = gtk_label_new(text);
    gtk_widget_add_css_class(lbl, "ns-chip");
    gtk_widget_add_css_class(lbl, variant);
    gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(lbl), 36);
    return lbl;
}

static GtkWidget *flow_box_new(void) {
    GtkWidget *fb = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(fb), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(fb), FALSE);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(fb), 12);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(fb), 6);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(fb), 6);
    return fb;
}

static void flow_box_clear(GtkWidget *fb) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(fb)))
        gtk_flow_box_remove(GTK_FLOW_BOX(fb), child);
}

/* Fills a flow box with chips; returns the number added. */
static int flow_box_fill(GtkWidget *fb, char **items, const char *variant) {
    flow_box_clear(fb);
    int n = 0;
    for (; items && items[n]; n++)
        gtk_flow_box_insert(GTK_FLOW_BOX(fb), chip_new(items[n], variant), -1);
    return n;
}

static GtkWidget *card_new(const char *title, GtkWidget *parent) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(card, "ns-card");
    GtkWidget *t = gtk_label_new(title);
    gtk_widget_add_css_class(t, "ns-card-title");
    gtk_label_set_xalign(GTK_LABEL(t), 0.0f);
    gtk_box_append(GTK_BOX(card), t);
    gtk_box_append(GTK_BOX(parent), card);
    return card;
}

static void set_class_from(GtkWidget *w, const char *const *all, const char *cls) {
    for (int i = 0; all[i]; i++) gtk_widget_remove_css_class(w, all[i]);
    gtk_widget_add_css_class(w, cls);
}

static const char *const SCORE_CLASSES[] = {
    "ns-score-aplus", "ns-score-a", "ns-score-b", "ns-score-c", "ns-score-d",
    "ns-score-e", "ns-score-unknown", NULL };
static const char *const NOVA_CLASSES[] = {
    "ns-nova-1", "ns-nova-2", "ns-nova-3", "ns-nova-4", "ns-nova-unknown", NULL };

GtkWidget *make_nutrient_row(const char *name, double value,
                              const char *unit, const char *level_key) {
    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(outer, "ns-nutrient-row");

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *name_lbl = gtk_label_new(name);
    gtk_widget_add_css_class(name_lbl, "ns-nutrient-name");
    gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
    gtk_widget_set_hexpand(name_lbl, TRUE);
    gtk_box_append(GTK_BOX(row), name_lbl);

    char *amount = format_amount(value);
    char *val_str = isnan(value) ? g_strdup(amount)        /* "—": unknown */
                                 : g_strdup_printf("%s %s", amount, unit);
    GtkWidget *val_lbl = gtk_label_new(val_str);
    gtk_widget_add_css_class(val_lbl, "ns-nutrient-value");
    gtk_box_append(GTK_BOX(row), val_lbl);
    g_free(amount);
    g_free(val_str);

    const char *level = level_key ? nutrient_level(level_key, value) : NULL;
    const char *lvl_css = NULL;
    if (level) {
        const char *text;
        if (strcmp(level, "LOW") == 0)      { lvl_css = "ns-level-low";  text = "Low"; }
        else if (strcmp(level, "MOD") == 0) { lvl_css = "ns-level-mod";  text = "Moderate"; }
        else                                { lvl_css = "ns-level-high"; text = "High"; }
        GtkWidget *pill = gtk_label_new(text);
        gtk_widget_add_css_class(pill, "ns-level");
        gtk_widget_add_css_class(pill, lvl_css);
        gtk_widget_set_size_request(pill, 70, -1);
        gtk_box_append(GTK_BOX(row), pill);
    }
    gtk_box_append(GTK_BOX(outer), row);

    if (lvl_css) {
        GtkWidget *bar = gtk_level_bar_new_for_interval(0.0, 1.0);
        /* Drop the default low/high/full offsets: colour comes from CSS. */
        gtk_level_bar_remove_offset_value(GTK_LEVEL_BAR(bar), GTK_LEVEL_BAR_OFFSET_LOW);
        gtk_level_bar_remove_offset_value(GTK_LEVEL_BAR(bar), GTK_LEVEL_BAR_OFFSET_HIGH);
        gtk_level_bar_remove_offset_value(GTK_LEVEL_BAR(bar), GTK_LEVEL_BAR_OFFSET_FULL);
        gtk_level_bar_set_value(GTK_LEVEL_BAR(bar),
                                MAX(0.02, nutrient_fraction(level_key, value)));
        gtk_widget_add_css_class(bar, "ns-bar");
        gtk_widget_add_css_class(bar, lvl_css);
        gtk_box_append(GTK_BOX(outer), bar);
    }
    return outer;
}

/* ────────────────────────────────────────────────
 *  History
 * ──────────────────────────────────────────────── */

static void on_history_activated(GtkListBox *lb, GtkListBoxRow *row, gpointer user_data) {
    (void)lb;
    AppState *state = user_data;
    int idx = gtk_list_box_row_get_index(row);
    if (idx < 0) return;
    GList *entry = g_list_nth(state->history, (guint)idx);
    if (!entry) return;
    state->current_product = entry->data;
    show_product(state, entry->data);   /* uses the cached image if any */
}

static GtkWidget *make_thumbnail(Product *p, int size) {
    GtkWidget *img;
    if (p->image) {
        img = gtk_image_new_from_paintable(GDK_PAINTABLE(p->image));
    } else {
        img = gtk_image_new_from_icon_name("image-x-generic-symbolic");
        gtk_widget_add_css_class(img, "ns-thumb-empty");
    }
    gtk_image_set_pixel_size(GTK_IMAGE(img), size);
    gtk_widget_set_size_request(img, size, size);
    gtk_widget_add_css_class(img, "ns-thumb");
    gtk_widget_set_overflow(img, GTK_OVERFLOW_HIDDEN);
    gtk_widget_set_valign(img, GTK_ALIGN_CENTER);
    return img;
}

static GtkWidget *make_history_row(Product *p) {
    GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(row_box, "ns-history-row");

    gtk_box_append(GTK_BOX(row_box), make_thumbnail(p, 44));

    GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(text_box, TRUE);
    gtk_widget_set_valign(text_box, GTK_ALIGN_CENTER);

    GtkWidget *name = gtk_label_new(p->product_name);
    gtk_widget_add_css_class(name, "ns-history-name");
    gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);

    const char *brand = (p->brand && p->brand[0]) ? p->brand : "Unknown brand";
    char *sub = (p->quantity && p->quantity[0])
        ? g_strdup_printf("%s · %s", brand, p->quantity) : g_strdup(brand);
    GtkWidget *brand_lbl = gtk_label_new(sub);
    g_free(sub);
    gtk_widget_add_css_class(brand_lbl, "ns-muted");
    gtk_label_set_xalign(GTK_LABEL(brand_lbl), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(brand_lbl), PANGO_ELLIPSIZE_END);

    gtk_box_append(GTK_BOX(text_box), name);
    gtk_box_append(GTK_BOX(text_box), brand_lbl);
    gtk_box_append(GTK_BOX(row_box), text_box);

    /* Nutri-Score mini badge */
    char upper[2] = { '?', 0 };
    if (p->nutriscore_grade) upper[0] = (char)g_ascii_toupper(p->nutriscore_grade[0]);
    GtkWidget *badge = gtk_label_new(upper);
    gtk_widget_add_css_class(badge, "ns-mini-badge");
    gtk_widget_add_css_class(badge, score_color(p->nutriscore_grade));
    gtk_widget_set_valign(badge, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(badge, score_description(p->nutriscore_grade));
    gtk_box_append(GTK_BOX(row_box), badge);

    return row_box;
}

Product *history_find(AppState *state, const char *barcode) {
    for (GList *l = state->history; l; l = l->next)
        if (g_strcmp0(((Product *)l->data)->barcode, barcode) == 0)
            return l->data;
    return NULL;
}

/* Rebuilds the "Recent scans" list; row index == position in history. */
void refresh_history(AppState *state) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(state->history_list)))
        gtk_list_box_remove(GTK_LIST_BOX(state->history_list), child);

    for (GList *l = state->history; l; l = l->next)
        gtk_list_box_insert(GTK_LIST_BOX(state->history_list),
                            make_history_row(l->data), -1);
    gtk_widget_set_visible(state->clear_history_button, state->history != NULL);
}

/* Takes ownership of p. Re-scanning a product moves it to the top instead of
 * duplicating it, and the list is bounded to LF_HISTORY_MAX entries. */
void history_add(AppState *state, Product *p) {
    GList *l = state->history;
    while (l) {
        GList *next = l->next;
        Product *old = l->data;
        if (old != p && g_strcmp0(old->barcode, p->barcode) == 0) {
            if (!p->image && old->image) p->image = g_object_ref(old->image);
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

void history_clear(AppState *state) {
    g_list_free_full(state->history, (GDestroyNotify)product_free);
    state->history = NULL;
    state->current_product = NULL;
    refresh_history(state);
}

static void on_clear_history_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    history_clear(user_data);
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


static void on_error_dismiss(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *state = user_data;
    gtk_revealer_set_reveal_child(GTK_REVEALER(state->error_revealer), FALSE);
}

static GtkWidget *icon_label_button(const char *icon, const char *text, const char *css) {
    GtkWidget *btn = gtk_button_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(box), gtk_image_new_from_icon_name(icon));
    gtk_box_append(GTK_BOX(box), gtk_label_new(text));
    gtk_button_set_child(GTK_BUTTON(btn), box);
    gtk_widget_add_css_class(btn, css);
    return btn;
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
        "Check the nutrition, processing and ingredients of any food product.");
    gtk_widget_add_css_class(hero_sub, "ns-hero-sub");
    gtk_label_set_xalign(GTK_LABEL(hero_sub), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(hero_sub), TRUE);

    /* Search row: entry + icon button */
    GtkWidget *scan_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_add_css_class(scan_row, "ns-scan-box");

    state->scan_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->scan_entry), "Enter a barcode");
    gtk_entry_set_input_purpose(GTK_ENTRY(state->scan_entry), GTK_INPUT_PURPOSE_DIGITS);
    gtk_entry_set_max_length(GTK_ENTRY(state->scan_entry), 48);
    gtk_widget_add_css_class(state->scan_entry, "ns-scan-entry");
    gtk_widget_set_hexpand(state->scan_entry, TRUE);
    gtk_widget_set_tooltip_text(state->scan_entry, "Barcode (Ctrl+L)");
    gtk_accessible_update_property(GTK_ACCESSIBLE(state->scan_entry),
        GTK_ACCESSIBLE_PROPERTY_LABEL, "Barcode", -1);
    g_signal_connect(state->scan_entry, "activate",
                     G_CALLBACK(on_scan_entry_activate), state);

    state->scan_button = gtk_button_new_from_icon_name("system-search-symbolic");
    gtk_widget_add_css_class(state->scan_button, "ns-search-btn");
    gtk_widget_set_tooltip_text(state->scan_button, "Look up (Enter)");
    gtk_accessible_update_property(GTK_ACCESSIBLE(state->scan_button),
        GTK_ACCESSIBLE_PROPERTY_LABEL, "Look up", -1);
    g_signal_connect(state->scan_button, "clicked", G_CALLBACK(on_scan_clicked), state);

    gtk_box_append(GTK_BOX(scan_row), state->scan_entry);
    gtk_box_append(GTK_BOX(scan_row), state->scan_button);

    state->camera_button = icon_label_button("camera-photo-symbolic",
                                             "Scan with camera", "ns-camera-btn");
    gtk_widget_set_tooltip_text(state->camera_button, "Scan with camera (Ctrl+K)");
    g_signal_connect(state->camera_button, "clicked",
                     G_CALLBACK(on_camera_scan_clicked), state);

    /* Examples */
    GtkWidget *hint = gtk_label_new("Try an example");
    gtk_widget_add_css_class(hint, "ns-hero-hint");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);

    GtkWidget *tips = flow_box_new();
    static const struct { const char *name, *barcode; } examples[] = {
        { "Nutella",   "3017620422003" },
        { "Coca-Cola", "5449000000996" },
        { "Evian",     "3068320123903" },
    };
    for (gsize i = 0; i < G_N_ELEMENTS(examples); i++) {
        GtkWidget *pill = gtk_button_new_with_label(examples[i].name);
        gtk_widget_add_css_class(pill, "ns-pill");
        gtk_widget_set_tooltip_text(pill, examples[i].barcode);
        g_object_set_data(G_OBJECT(pill), "barcode", (gpointer)examples[i].barcode);
        g_signal_connect(pill, "clicked", G_CALLBACK(on_example_clicked), state);
        gtk_flow_box_insert(GTK_FLOW_BOX(tips), pill, -1);
    }

    gtk_box_append(GTK_BOX(hero), hero_title);
    gtk_box_append(GTK_BOX(hero), hero_sub);
    gtk_box_append(GTK_BOX(hero), scan_row);
    gtk_box_append(GTK_BOX(hero), state->camera_button);
    gtk_box_append(GTK_BOX(hero), hint);
    gtk_box_append(GTK_BOX(hero), tips);
    gtk_box_append(GTK_BOX(vbox), hero);

    /* ── Inline error banner (replaces modal dialogs) ── */
    state->error_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(state->error_revealer),
        GTK_REVEALER_TRANSITION_TYPE_SLIDE_DOWN);
    /* ALERT role: screen readers announce it when shown. (Not
     * gtk_accessible_announce(): it crashes when no a11y backend is active.) */
    GtkWidget *banner = g_object_new(GTK_TYPE_BOX,
        "orientation", GTK_ORIENTATION_HORIZONTAL, "spacing", 10,
        "accessible-role", GTK_ACCESSIBLE_ROLE_ALERT, NULL);
    gtk_widget_add_css_class(banner, "ns-error-banner");
    GtkWidget *warn = gtk_image_new_from_icon_name("dialog-warning-symbolic");
    gtk_widget_set_valign(warn, GTK_ALIGN_START);
    state->error_label = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(state->error_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(state->error_label), 0.0f);
    gtk_label_set_selectable(GTK_LABEL(state->error_label), TRUE);
    gtk_widget_set_hexpand(state->error_label, TRUE);
    GtkWidget *dismiss = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(dismiss, "flat");
    gtk_widget_add_css_class(dismiss, "ns-banner-close");
    gtk_widget_set_valign(dismiss, GTK_ALIGN_START);
    gtk_widget_set_tooltip_text(dismiss, "Dismiss");
    g_signal_connect(dismiss, "clicked", G_CALLBACK(on_error_dismiss), state);
    gtk_box_append(GTK_BOX(banner), warn);
    gtk_box_append(GTK_BOX(banner), state->error_label);
    gtk_box_append(GTK_BOX(banner), dismiss);
    gtk_revealer_set_child(GTK_REVEALER(state->error_revealer), banner);
    gtk_box_append(GTK_BOX(vbox), state->error_revealer);

    /* ── History section ── */
    GtkWidget *hist_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(hist_header, "ns-section-header");
    GtkWidget *hist_title = gtk_label_new("Recent scans");
    gtk_widget_add_css_class(hist_title, "ns-section-title");
    gtk_label_set_xalign(GTK_LABEL(hist_title), 0.0f);
    gtk_widget_set_hexpand(hist_title, TRUE);
    state->clear_history_button = gtk_button_new_with_label("Clear");
    gtk_widget_add_css_class(state->clear_history_button, "ns-text-btn");
    gtk_widget_set_tooltip_text(state->clear_history_button, "Clear recent scans");
    gtk_widget_set_visible(state->clear_history_button, FALSE);
    g_signal_connect(state->clear_history_button, "clicked",
                     G_CALLBACK(on_clear_history_clicked), state);
    gtk_box_append(GTK_BOX(hist_header), hist_title);
    gtk_box_append(GTK_BOX(hist_header), state->clear_history_button);
    gtk_box_append(GTK_BOX(vbox), hist_header);

    state->history_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(state->history_list),
        GTK_SELECTION_NONE);
    gtk_widget_add_css_class(state->history_list, "ns-history-list");
    gtk_accessible_update_property(GTK_ACCESSIBLE(state->history_list),
        GTK_ACCESSIBLE_PROPERTY_LABEL, "Recent scans", -1);
    g_signal_connect(state->history_list, "row-activated",
                     G_CALLBACK(on_history_activated), state);

    GtkWidget *empty_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_halign(empty_box, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(empty_box, "ns-empty");
    GtkWidget *empty_icon = gtk_image_new_from_icon_name("view-list-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(empty_icon), 40);
    gtk_widget_add_css_class(empty_icon, "ns-empty-icon");
    GtkWidget *empty_text = gtk_label_new("No products scanned yet");
    gtk_widget_add_css_class(empty_text, "ns-empty-text");
    GtkWidget *empty_sub = gtk_label_new("Products you look up will appear here.");
    gtk_widget_add_css_class(empty_sub, "ns-muted");
    gtk_box_append(GTK_BOX(empty_box), empty_icon);
    gtk_box_append(GTK_BOX(empty_box), empty_text);
    gtk_box_append(GTK_BOX(empty_box), empty_sub);
    gtk_list_box_set_placeholder(GTK_LIST_BOX(state->history_list), empty_box);

    gtk_box_append(GTK_BOX(vbox), state->history_list);
    return scroll;
}

/* ────────────────────────────────────────────────
 *  Loading page
 * ──────────────────────────────────────────────── */

static void on_loading_cancel(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *state = user_data;
    cancel_network(state);
    show_home(state);
}

GtkWidget *build_loading_page(AppState *state) {
    GtkWidget *center = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_halign(center, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(center, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(center, TRUE);

    state->spinner = gtk_spinner_new();
    gtk_widget_set_size_request(state->spinner, 48, 48);

    state->loading_label = gtk_label_new("Looking up product…");
    gtk_widget_add_css_class(state->loading_label, "ns-loading-label");

    GtkWidget *cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(cancel, "ns-text-btn");
    gtk_widget_set_halign(cancel, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(cancel, "Cancel (Esc)");
    g_signal_connect(cancel, "clicked", G_CALLBACK(on_loading_cancel), state);

    gtk_box_append(GTK_BOX(center), state->spinner);
    gtk_box_append(GTK_BOX(center), state->loading_label);
    gtk_box_append(GTK_BOX(center), cancel);
    return center;
}

/* ────────────────────────────────────────────────
 *  Product page
 * ──────────────────────────────────────────────── */

/* One of the three score tiles: title, coloured badge, caption. */
static GtkWidget *score_tile(const char *title, const char *badge_css,
                             GtkWidget **badge, GtkWidget **value, GtkWidget **desc) {
    GtkWidget *tile = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(tile, "ns-score-tile");

    GtkWidget *t = gtk_label_new(title);
    gtk_widget_add_css_class(t, "ns-tile-title");

    *badge = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(*badge, badge_css);
    gtk_widget_set_halign(*badge, GTK_ALIGN_CENTER);
    gtk_widget_set_size_request(*badge, 52, 52);

    *value = gtk_label_new("?");
    gtk_widget_add_css_class(*value, "ns-badge-value");
    gtk_widget_set_vexpand(*value, TRUE);
    gtk_widget_set_valign(*value, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(*badge), *value);

    *desc = gtk_label_new("");
    gtk_widget_add_css_class(*desc, "ns-tile-desc");
    gtk_label_set_wrap(GTK_LABEL(*desc), TRUE);
    gtk_label_set_justify(GTK_LABEL(*desc), GTK_JUSTIFY_CENTER);

    gtk_box_append(GTK_BOX(tile), t);
    gtk_box_append(GTK_BOX(tile), *badge);
    gtk_box_append(GTK_BOX(tile), *desc);
    return tile;
}

GtkWidget *build_product_page(AppState *state) {
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(vbox, "ns-product-page");
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), vbox);

    /* ── Image ── */
    state->product_image = gtk_image_new_from_icon_name("image-x-generic-symbolic");
    gtk_image_set_pixel_size(GTK_IMAGE(state->product_image), 180);
    gtk_widget_set_halign(state->product_image, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(state->product_image, "ns-product-image");
    gtk_widget_set_overflow(state->product_image, GTK_OVERFLOW_HIDDEN);
    gtk_box_append(GTK_BOX(vbox), state->product_image);

    /* ── Name, brand · quantity ── */
    GtkWidget *meta_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(meta_box, "ns-meta");
    state->brand_label = label_new_styled("", "ns-product-brand");
    state->product_name_label = label_new_styled("", "ns-product-name");
    gtk_box_append(GTK_BOX(meta_box), state->product_name_label);
    gtk_box_append(GTK_BOX(meta_box), state->brand_label);
    gtk_box_append(GTK_BOX(vbox), meta_box);

    /* ── Scores ── */
    GtkWidget *score_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_set_homogeneous(GTK_BOX(score_row), TRUE);
    gtk_widget_add_css_class(score_row, "ns-score-row");
    gtk_box_append(GTK_BOX(score_row), score_tile("Nutri-Score", "ns-score-badge",
        &state->score_badge, &state->score_letter, &state->score_desc));
    gtk_box_append(GTK_BOX(score_row), score_tile("NOVA", "ns-nova-badge",
        &state->nova_badge, &state->nova_num, &state->nova_desc));
    gtk_box_append(GTK_BOX(score_row), score_tile("Eco-Score", "ns-score-badge",
        &state->eco_badge, &state->eco_letter, &state->eco_desc));
    gtk_box_append(GTK_BOX(vbox), score_row);

    /* ── Nutrition ── */
    GtkWidget *nut_card = card_new("Nutrition facts", vbox);
    GtkWidget *per = gtk_label_new("per 100 g / 100 ml");
    gtk_widget_add_css_class(per, "ns-muted");
    gtk_label_set_xalign(GTK_LABEL(per), 0.0f);
    gtk_box_append(GTK_BOX(nut_card), per);
    state->nutrition_grid = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(nut_card), state->nutrition_grid);

    /* ── Allergens ── */
    GtkWidget *all_card = card_new("Allergens", vbox);
    state->allergens_box = flow_box_new();
    gtk_box_append(GTK_BOX(all_card), state->allergens_box);
    state->traces_label = label_new_styled("", "ns-muted");
    gtk_box_append(GTK_BOX(all_card), state->traces_label);

    /* ── Labels (hidden when none) ── */
    state->labels_card = card_new("Labels", vbox);
    state->labels_box = flow_box_new();
    gtk_box_append(GTK_BOX(state->labels_card), state->labels_box);

    /* ── Additives ── */
    GtkWidget *add_card = card_new("Additives", vbox);
    state->additives_box = flow_box_new();
    gtk_box_append(GTK_BOX(add_card), state->additives_box);

    /* ── Ingredients ── */
    GtkWidget *ing_card = card_new("Ingredients", vbox);
    state->ingredients_label = label_new_styled("", "ns-body");
    gtk_label_set_selectable(GTK_LABEL(state->ingredients_label), TRUE);
    gtk_box_append(GTK_BOX(ing_card), state->ingredients_label);

    /* ── Source link ── */
    state->off_link = gtk_link_button_new_with_label(
        "https://world.openfoodfacts.org", "View on OpenFoodFacts");
    gtk_widget_add_css_class(state->off_link, "ns-off-link");
    gtk_widget_set_halign(state->off_link, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(vbox), state->off_link);

    return scroll;
}

/* ────────────────────────────────────────────────
 *  Navigation
 * ──────────────────────────────────────────────── */

static void on_back_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *state = user_data;
    /* While loading, "back" cancels the lookup. */
    cancel_network(state);
    show_home(state);
}

void show_home(AppState *state) {
    gtk_spinner_stop(GTK_SPINNER(state->spinner));
    gtk_label_set_text(GTK_LABEL(state->title_label), "librefood");
    gtk_widget_set_visible(state->back_button, FALSE);
    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "home");
}

void show_error(AppState *state, const char *msg) {
    const char *text = msg ? msg : "Product not found. Check the barcode and try again.";
    gtk_label_set_text(GTK_LABEL(state->error_label), text);
    gtk_revealer_set_reveal_child(GTK_REVEALER(state->error_revealer), TRUE);
    show_home(state);
}

void show_product(AppState *state, Product *p) {
    if (!p) {
        show_error(state, "Product not found in the OpenFoodFacts database.");
        return;
    }
    gtk_revealer_set_reveal_child(GTK_REVEALER(state->error_revealer), FALSE);

    /* ── Meta ── */
    gtk_label_set_text(GTK_LABEL(state->product_name_label), p->product_name);
    const char *brand = (p->brand && p->brand[0]) ? p->brand : "Unknown brand";
    char *sub = (p->quantity && p->quantity[0])
        ? g_strdup_printf("%s · %s", brand, p->quantity) : g_strdup(brand);
    gtk_label_set_text(GTK_LABEL(state->brand_label), sub);
    g_free(sub);

    /* ── Nutri-Score ── */
    set_class_from(state->score_badge, SCORE_CLASSES, score_color(p->nutriscore_grade));
    char upper[2] = { '?', 0 };
    if (p->nutriscore_grade) upper[0] = (char)g_ascii_toupper(p->nutriscore_grade[0]);
    gtk_label_set_text(GTK_LABEL(state->score_letter), upper);
    gtk_label_set_text(GTK_LABEL(state->score_desc), score_description(p->nutriscore_grade));

    /* ── NOVA ── */
    char nova_css[20] = "ns-nova-unknown";
    if (p->nova_group)
        snprintf(nova_css, sizeof(nova_css), "ns-nova-%c", p->nova_group[0]);
    set_class_from(state->nova_badge, NOVA_CLASSES, nova_css);
    gtk_label_set_text(GTK_LABEL(state->nova_num), p->nova_group ? p->nova_group : "?");
    gtk_label_set_text(GTK_LABEL(state->nova_desc), nova_description(p->nova_group));

    /* ── Eco-Score ── */
    set_class_from(state->eco_badge, SCORE_CLASSES, ecoscore_color(p->ecoscore_grade));
    gtk_label_set_text(GTK_LABEL(state->eco_letter), ecoscore_label(p->ecoscore_grade));
    gtk_label_set_text(GTK_LABEL(state->eco_desc), ecoscore_description(p->ecoscore_grade));

    /* ── Nutrition ── */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(state->nutrition_grid)))
        gtk_box_remove(GTK_BOX(state->nutrition_grid), child);

    struct { const char *name; double val; const char *unit; const char *key; } rows[] = {
        { "Energy",          p->energy_kcal,   "kcal", NULL            },
        { "Fat",             p->fat,           "g",    "fat"           },
        { "  Saturated fat", p->saturated_fat, "g",    "saturated_fat" },
        { "Carbohydrates",   p->carbs,         "g",    NULL            },
        { "  Sugars",        p->sugars,        "g",    "sugars"        },
        { "Fiber",           p->fiber,         "g",    NULL            },
        { "Protein",         p->protein,       "g",    NULL            },
        { "Salt",            p->salt,          "g",    "salt"          },
    };
    for (gsize i = 0; i < G_N_ELEMENTS(rows); i++)
        gtk_box_append(GTK_BOX(state->nutrition_grid),
            make_nutrient_row(rows[i].name, rows[i].val, rows[i].unit, rows[i].key));

    /* ── Allergens & traces ── */
    if (flow_box_fill(state->allergens_box, p->allergens, "ns-chip-warn") == 0) {
        GtkWidget *none = gtk_label_new("None declared");
        gtk_widget_add_css_class(none, "ns-muted");
        gtk_flow_box_insert(GTK_FLOW_BOX(state->allergens_box), none, -1);
    }
    gboolean has_traces = p->traces && p->traces[0];
    if (has_traces) {
        char *joined = g_strjoinv(", ", p->traces);
        char *txt = g_strdup_printf("May contain traces of: %s", joined);
        gtk_label_set_text(GTK_LABEL(state->traces_label), txt);
        g_free(txt);
        g_free(joined);
    }
    gtk_widget_set_visible(state->traces_label, has_traces);

    /* ── Labels ── */
    gtk_widget_set_visible(state->labels_card,
        flow_box_fill(state->labels_box, p->labels, "ns-chip-good") > 0);

    /* ── Additives ── */
    flow_box_clear(state->additives_box);
    if (p->additives_count == 0) {
        GtkWidget *none = gtk_label_new("No additives listed");
        gtk_widget_add_css_class(none, "ns-muted");
        gtk_flow_box_insert(GTK_FLOW_BOX(state->additives_box), none, -1);
    }
    for (int i = 0; i < p->additives_count; i++) {
        char *txt = additive_display(p->additives[i]);
        gtk_flow_box_insert(GTK_FLOW_BOX(state->additives_box),
                            chip_new(txt, "ns-chip-additive"), -1);
        g_free(txt);
    }

    /* ── Ingredients ── */
    gtk_label_set_text(GTK_LABEL(state->ingredients_label),
        (p->ingredients_text && p->ingredients_text[0]) ?
        p->ingredients_text : "Not available.");

    /* ── Source link (barcode is digits-only, safe in a URL) ── */
    char *uri = g_strdup_printf("https://world.openfoodfacts.org/product/%s", p->barcode);
    gtk_link_button_set_uri(GTK_LINK_BUTTON(state->off_link), uri);
    gtk_widget_set_tooltip_text(state->off_link, uri);
    g_free(uri);

    /* ── Image: placeholder, then cached texture or download ── */
    gtk_image_set_from_icon_name(GTK_IMAGE(state->product_image),
        "image-x-generic-symbolic");
    fetch_image(state, p);

    /* ── Switch to product page, scrolled to top ── */
    char *title = g_utf8_strlen(p->product_name, -1) > 32
        ? g_utf8_substring(p->product_name, 0, 32)
        : g_strdup(p->product_name);
    gtk_label_set_text(GTK_LABEL(state->title_label), title);
    g_free(title);
    gtk_spinner_stop(GTK_SPINNER(state->spinner));
    gtk_widget_set_visible(state->back_button, TRUE);
    GtkWidget *page = gtk_stack_get_child_by_name(GTK_STACK(state->stack), "product");
    gtk_adjustment_set_value(
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(page)), 0);
    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "product");
    /* Keep focus off the selectable ingredients text (it would select-all). */
    gtk_widget_grab_focus(state->back_button);
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

    gtk_revealer_set_reveal_child(GTK_REVEALER(state->error_revealer), FALSE);
    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "loading");
    gtk_spinner_start(GTK_SPINNER(state->spinner));
    gtk_widget_set_visible(state->back_button, TRUE);   /* back = cancel */
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


/* ────────────────────────────────────────────────
 *  Keyboard actions
 * ──────────────────────────────────────────────── */

static void act_focus_search(GSimpleAction *a, GVariant *v, gpointer user_data) {
    (void)a; (void)v;
    AppState *state = user_data;
    if (g_strcmp0(gtk_stack_get_visible_child_name(GTK_STACK(state->stack)), "loading") != 0)
        show_home(state);
    gtk_widget_grab_focus(state->scan_entry);
}

static void act_back(GSimpleAction *a, GVariant *v, gpointer user_data) {
    (void)a; (void)v;
    AppState *state = user_data;
    const char *page = gtk_stack_get_visible_child_name(GTK_STACK(state->stack));
    if (g_strcmp0(page, "home") == 0) {
        /* Esc on home dismisses the error banner, if any. */
        gtk_revealer_set_reveal_child(GTK_REVEALER(state->error_revealer), FALSE);
        return;
    }
    on_back_clicked(NULL, state);
}

static void act_scan_camera(GSimpleAction *a, GVariant *v, gpointer user_data) {
    (void)a; (void)v;
    AppState *state = user_data;
    if (g_strcmp0(gtk_stack_get_visible_child_name(GTK_STACK(state->stack)), "loading") == 0)
        return;
    on_camera_scan_clicked(NULL, state);
}

static void act_quit(GSimpleAction *a, GVariant *v, gpointer user_data) {
    (void)a; (void)v;
    AppState *state = user_data;
    gtk_window_destroy(GTK_WINDOW(state->window));
}

static void install_actions(AppState *state) {
    const GActionEntry win_entries[] = {
        { .name = "focus-search", .activate = act_focus_search },
        { .name = "back",         .activate = act_back },
        { .name = "scan-camera",  .activate = act_scan_camera },
        { .name = "quit",         .activate = act_quit },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(state->window), win_entries,
                                    G_N_ELEMENTS(win_entries), state);

    static const struct { const char *action; const char *accels[3]; } accels[] = {
        { "win.focus-search", { "<Control>l", "<Control>f", NULL } },
        { "win.back",         { "Escape", "<Alt>Left", NULL } },
        { "win.scan-camera",  { "<Control>k", NULL } },
        { "win.quit",         { "<Control>q", "<Control>w", NULL } },
    };
    for (gsize i = 0; i < G_N_ELEMENTS(accels); i++)
        gtk_application_set_accels_for_action(state->app, accels[i].action,
                                              accels[i].accels);
}

/* ────────────────────────────────────────────────
 *  App activation
 * ──────────────────────────────────────────────── */

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
    gtk_window_set_default_size(GTK_WINDOW(state->window), 420, 760);
    gtk_widget_set_size_request(state->window, 340, 480);
    g_signal_connect(state->window, "destroy", G_CALLBACK(on_main_window_destroy), state);

    /* Header bar */
    state->header_bar = gtk_header_bar_new();
    gtk_widget_add_css_class(state->header_bar, "ns-header");

    state->back_button = gtk_button_new_from_icon_name("go-previous-symbolic");
    gtk_widget_add_css_class(state->back_button, "ns-back-btn");
    gtk_widget_set_tooltip_text(state->back_button, "Back (Esc)");
    gtk_accessible_update_property(GTK_ACCESSIBLE(state->back_button),
        GTK_ACCESSIBLE_PROPERTY_LABEL, "Back", -1);
    gtk_widget_set_visible(state->back_button, FALSE);
    g_signal_connect(state->back_button, "clicked", G_CALLBACK(on_back_clicked), state);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(state->header_bar), state->back_button);

    state->title_label = gtk_label_new("librefood");
    gtk_widget_add_css_class(state->title_label, "ns-title");
    gtk_label_set_ellipsize(GTK_LABEL(state->title_label), PANGO_ELLIPSIZE_END);
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(state->header_bar), state->title_label);
    gtk_window_set_titlebar(GTK_WINDOW(state->window), state->header_bar);

    /* Stack */
    state->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(state->stack),
        GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(state->stack), 200);

    gtk_stack_add_named(GTK_STACK(state->stack), build_home_page(state), "home");
    gtk_stack_add_named(GTK_STACK(state->stack), build_loading_page(state), "loading");
    gtk_stack_add_named(GTK_STACK(state->stack), build_product_page(state), "product");

    gtk_stack_set_visible_child_name(GTK_STACK(state->stack), "home");
    gtk_window_set_child(GTK_WINDOW(state->window), state->stack);
    install_actions(state);
    gtk_window_present(GTK_WINDOW(state->window));
    gtk_widget_grab_focus(state->scan_entry);
}
