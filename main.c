/*
 * librefood - A Yuka-like product scanner for Linux
 * Built with GTK4 + libsoup3 + OpenFoodFacts API
 *
 * Compile: see build.sh
 */

#include "librefood.h"

int main(int argc, char *argv[]) {
    GtkApplication *app;
    int status;

    app = gtk_application_new("org.librefood.app", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
