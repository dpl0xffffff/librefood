/*
 * Integration tests: drives the real UI/network/camera code headlessly
 * (GDK_BACKEND=broadway) against tests/fake_off_server.py and
 * tests/fake_scanner.py. Run via tests/run_tests.sh.
 */
#include "../librefood.h"
#include <signal.h>
#include <errno.h>
#include <glib/gstdio.h>

static AppState *S;
static int failures;

#define CHECK(cond, ...) do { \
    if (cond) { g_print("ok   - "); g_print(__VA_ARGS__); g_print("\n"); } \
    else { failures++; g_print("FAIL - "); g_print(__VA_ARGS__); \
           g_print("   (%s:%d: %s)\n", __FILE__, __LINE__, #cond); } \
} while (0)

static const char *page(void) {
    return gtk_stack_get_visible_child_name(GTK_STACK(S->stack));
}
static const char *title(void) {
    return gtk_label_get_text(GTK_LABEL(S->title_label));
}

/* Spins the main loop until cond() holds or secs elapse. */
static gboolean wait_for(gboolean (*cond)(void), double secs) {
    gint64 end = g_get_monotonic_time() + (gint64)(secs * G_USEC_PER_SEC);
    while (!cond()) {
        if (g_get_monotonic_time() > end) return FALSE;
        g_main_context_iteration(NULL, FALSE);
        g_usleep(1000);
    }
    return TRUE;
}
static gboolean c_false(void) { return FALSE; }
static void spin(double secs) { wait_for(c_false, secs); }
static gboolean c_home(void)    { return g_strcmp0(page(), "home") == 0; }
static gboolean c_product(void) { return g_strcmp0(page(), "product") == 0; }
static gboolean c_cam_idle(void){ return S->camera_proc == NULL; }

static gboolean lookup_to(const char *bc, gboolean (*cond)(void), double secs) {
    show_home(S);
    if (!do_lookup(S, bc)) return FALSE;
    return wait_for(cond, secs);
}

static pid_t read_pid(const char *path) {
    char *txt = NULL;
    for (int i = 0; i < 200 && !txt; i++) {
        if (!g_file_get_contents(path, &txt, NULL, NULL) || !*txt) {
            g_clear_pointer(&txt, g_free);
            spin(0.02);
        }
    }
    pid_t pid = txt ? (pid_t)atoi(txt) : 0;
    g_free(txt);
    return pid;
}
static gboolean pid_gone(pid_t pid) {
    for (int i = 0; i < 200; i++) {
        if (kill(pid, 0) != 0 && errno == ESRCH) return TRUE;
        spin(0.01);
    }
    return FALSE;
}

static void run_tests(GtkApplication *app, gpointer pidfile) {
    S = g_object_get_data(G_OBJECT(app), "librefood-state");
    g_assert_nonnull(S);

    /* ── input validation ── */
    CHECK(!do_lookup(S, "abc") && c_home(), "non-numeric input rejected, stays home");
    CHECK(!do_lookup(S, "../x") && c_home(), "path-like input rejected");

    /* ── happy path ── */
    CHECK(lookup_to("30176204", c_product, 5), "valid product shows product page");
    CHECK(g_strcmp0(title(), "Nutella") == 0, "title is product name (%s)", title());
    CHECK(g_strcmp0(gtk_label_get_text(GTK_LABEL(S->nova_desc)), "Ultra-processed") == 0,
          "NOVA description populated");
    CHECK(g_list_length(S->history) == 1, "history has 1 entry");
    CHECK(lookup_to("3017 6204", c_product, 5), "spaces in barcode accepted");
    CHECK(g_list_length(S->history) == 1, "re-scan does not duplicate history");

    /* ── failure responses must return to home, not hang on spinner ── */
    CHECK(lookup_to("11111111", c_home, 5), "not-found (status 0) returns home");
    CHECK(lookup_to("22222222", c_home, 5), "HTTP 503 returns home");
    CHECK(lookup_to("33333333", c_home, 5), "malformed JSON returns home");
    CHECK(lookup_to("77777777", c_home, 5), "non-object JSON root returns home");
    CHECK(lookup_to("99999999", c_home, 5), "HTTP 404 returns home");
    CHECK(g_list_length(S->history) == 1, "failed lookups don't touch history");

    /* ── null-heavy product ── */
    CHECK(lookup_to("44444444", c_product, 5), "all-null product renders");
    CHECK(g_strcmp0(title(), "Unknown product") == 0, "fallback name used");
    CHECK(g_list_length(S->history) == 2, "history has 2 entries");

    /* ── timeout ── */
    g_object_set(S->http_session, "timeout", 1, NULL);
    gint64 t0 = g_get_monotonic_time();
    CHECK(lookup_to("55555555", c_home, 4), "slow server times out back to home");
    g_print("       (timed out after %.1fs)\n", (g_get_monotonic_time() - t0) / 1e6);
    g_object_set(S->http_session, "timeout", LF_HTTP_TIMEOUT_SEC, NULL);

    /* ── race: slow lookup superseded by a fast one ── */
    show_home(S);
    do_lookup(S, "66666666");
    do_lookup(S, "30176204");
    CHECK(wait_for(c_product, 5) && g_strcmp0(title(), "Nutella") == 0,
          "newer lookup wins");
    spin(2.5);  /* slow response would land now if not cancelled */
    CHECK(g_strcmp0(title(), "Nutella") == 0, "stale slow response ignored");
    gboolean has_slow = FALSE;
    for (GList *l = S->history; l; l = l->next)
        if (g_strcmp0(((Product *)l->data)->product_name, "Slow") == 0) has_slow = TRUE;
    CHECK(!has_slow, "stale response not added to history");

    /* ── history row activation (previously crashed) ── */
    show_home(S);
    GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(S->history_list), 1);
    CHECK(row != NULL, "history row 1 exists");
    if (row) {
        Product *expect = g_list_nth_data(S->history, 1);
        g_signal_emit_by_name(S->history_list, "row-activated", row);
        spin(0.3);
        CHECK(c_product() && g_strcmp0(title(), expect->product_name) == 0,
              "history activation shows that product (%s)", title());
    }

    /* ── camera: successful scan ── */
    show_home(S);
    g_setenv("FAKE_SCANNER_MODE", "barcode", TRUE);
    g_signal_emit_by_name(S->camera_button, "clicked");
    CHECK(S->camera_dialog != NULL, "camera dialog opened");
    CHECK(wait_for(c_product, 5) && S->camera_dialog == NULL && S->camera_proc == NULL,
          "scanned barcode looked up, dialog and process cleaned up");
    CHECK(g_strcmp0(title(), "Nutella") == 0, "stderr digits not mistaken for barcode");

    /* ── camera: scanner reports error ── */
    show_home(S);
    g_setenv("FAKE_SCANNER_MODE", "error", TRUE);
    g_signal_emit_by_name(S->camera_button, "clicked");
    CHECK(wait_for(c_cam_idle, 5), "error: scanner process released");
    CHECK(S->camera_status_label &&
          g_str_has_prefix(gtk_label_get_text(GTK_LABEL(S->camera_status_label)), "ERROR:"),
          "error message shown in dialog");
    if (S->camera_dialog) gtk_window_destroy(GTK_WINDOW(S->camera_dialog));
    CHECK(S->camera_dialog == NULL && S->camera_status_label == NULL,
          "dialog state cleared on destroy");

    /* ── camera: Cancel kills a running scanner; reopen works ── */
    g_setenv("FAKE_SCANNER_MODE", "hang", TRUE);
    g_remove(pidfile);
    g_signal_emit_by_name(S->camera_button, "clicked");
    pid_t pid = read_pid(pidfile);
    CHECK(pid > 0, "hanging scanner started (pid %d)", (int)pid);
    gtk_window_destroy(GTK_WINDOW(S->camera_dialog));   /* what Cancel does */
    CHECK(S->camera_dialog == NULL && S->camera_proc == NULL, "cancel clears state");
    CHECK(pid > 0 && pid_gone(pid), "cancel kills scanner process");
    g_signal_emit_by_name(S->camera_button, "clicked"); /* old use-after-free */
    CHECK(S->camera_dialog != NULL, "camera dialog can be reopened after cancel");
    if (S->camera_dialog) gtk_window_destroy(GTK_WINDOW(S->camera_dialog));

    /* ── shutdown with scanner running ── */
    g_remove(pidfile);
    g_signal_emit_by_name(S->camera_button, "clicked");
    pid = read_pid(pidfile);
    show_home(S);
    do_lookup(S, "66666666");                 /* request in flight at exit */
    gtk_window_destroy(GTK_WINDOW(S->window));
    CHECK(pid > 0 && pid_gone(pid), "closing main window kills scanner");
    CHECK(S->camera_proc == NULL && S->fetch_cancel == NULL, "shutdown cancels work");
    spin(2.0);                                /* let cancelled callbacks run */
    CHECK(TRUE, "no crash from callbacks after shutdown");
}

int main(int argc, char **argv) {
    if (argc < 2) { g_printerr("usage: %s PIDFILE\n", argv[0]); return 2; }
    GtkApplication *app = gtk_application_new("org.librefood.test",
                                              G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect_after(app, "activate", G_CALLBACK(run_tests), argv[1]);
    char *args[] = { argv[0], NULL };
    g_application_run(G_APPLICATION(app), 1, args);
    g_object_unref(app);
    g_print("\n%s (%d failure%s)\n", failures ? "FAILED" : "PASSED",
            failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
