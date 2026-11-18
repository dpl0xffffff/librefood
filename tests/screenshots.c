/*
 * Renders the app's pages to PNG files headlessly, in light and dark mode,
 * for visual review. Run via: tests/run_tests.sh --screenshots DIR
 */
#include "../librefood.h"

static AppState *S;
static const char *outdir;

static gboolean c_false(void) { return FALSE; }
static gboolean wait_for(gboolean (*cond)(void), double secs) {
    gint64 end = g_get_monotonic_time() + (gint64)(secs * G_USEC_PER_SEC);
    while (!cond()) {
        if (g_get_monotonic_time() > end) return FALSE;
        g_main_context_iteration(NULL, FALSE);
        g_usleep(1000);
    }
    return TRUE;
}
static void spin(double secs) { wait_for(c_false, secs); }
static gboolean c_product(void) {
    return g_strcmp0(gtk_stack_get_visible_child_name(GTK_STACK(S->stack)), "product") == 0;
}

static void save_png(const char *name) {
    spin(0.6);   /* let stack transitions and images settle */
    GtkWidget *w = S->window;
    /* Broadway's frame clock stalls without a connected browser, so lay the
     * window out by hand at its default size before snapshotting. */
    int wd = 420, ht = 760;
    gtk_widget_allocate(w, wd, ht, -1, NULL);
    /* GtkWidgetPaintable renders nothing for a toplevel, so invoke the
     * window's own snapshot vfunc (background, titlebar, children). */
    GtkSnapshot *snap = gtk_snapshot_new();
    GTK_WIDGET_GET_CLASS(w)->snapshot(w, snap);
    GskRenderNode *node = gtk_snapshot_free_to_node(snap);
    GskRenderer *r = gsk_cairo_renderer_new();
    gsk_renderer_realize_for_display(r, gdk_display_get_default(), NULL);
    graphene_rect_t rect = GRAPHENE_RECT_INIT(0, 0, wd, ht);
    GdkTexture *tex = gsk_renderer_render_texture(r, node, &rect);
    char *path = g_strdup_printf("%s/%s.png", outdir, name);
    gdk_texture_save_to_png(tex, path);
    g_print("wrote %s (%dx%d)\n", path, wd, ht);
    g_free(path);
    g_object_unref(tex);
    gsk_renderer_unrealize(r);
    g_object_unref(r);
    gsk_render_node_unref(node);
}

static void shoot(const char *suffix) {
    char name[64];
    show_home(S);
    snprintf(name, sizeof name, "home-%s", suffix);
    save_png(name);

    do_lookup(S, "30176204");
    wait_for(c_product, 5);
    snprintf(name, sizeof name, "product-%s", suffix);
    save_png(name);

    GtkWidget *scroll = gtk_stack_get_child_by_name(GTK_STACK(S->stack), "product");
    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
    gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));
    snprintf(name, sizeof name, "product-bottom-%s", suffix);
    save_png(name);
    gtk_adjustment_set_value(adj, 0);

    show_home(S);
    do_lookup(S, "abc");          /* error presentation */
    snprintf(name, sizeof name, "error-%s", suffix);
    save_png(name);
}

static void run(GtkApplication *app, gpointer data) {
    (void)data;
    S = g_object_get_data(G_OBJECT(app), "librefood-state");
    gtk_window_set_default_size(GTK_WINDOW(S->window), 420, 760);
    g_object_set(gtk_settings_get_default(), "gtk-enable-animations", FALSE, NULL);
    spin(0.5);
    /* Two lookups first so history has entries in the home shot. */
    do_lookup(S, "44444444"); wait_for(c_product, 5);
    shoot("light");

    GtkSettings *settings = gtk_settings_get_default();
    g_object_set(settings, "gtk-interface-color-scheme", GTK_INTERFACE_COLOR_SCHEME_DARK, NULL);
    shoot("dark");
    gtk_window_destroy(GTK_WINDOW(S->window));
}

int main(int argc, char **argv) {
    if (argc < 2) { g_printerr("usage: %s OUTDIR\n", argv[0]); return 2; }
    outdir = argv[1];
    GtkApplication *app = gtk_application_new("org.librefood.shots",
                                              G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect_after(app, "activate", G_CALLBACK(run), NULL);
    char *args[] = { argv[0], NULL };
    int st = g_application_run(G_APPLICATION(app), 1, args);
    g_object_unref(app);
    return st;
}
