#include "librefood.h"

#define OFF_API "https://world.openfoodfacts.org/api/v0/product/%s.json"

typedef struct {
    AppState *state;
    char     *barcode;
} FetchCtx;

static void on_image_ready(GObject *source, GAsyncResult *res, gpointer user_data) {
    GtkWidget *image_widget = GTK_WIDGET(user_data);
    SoupSession *session = SOUP_SESSION(source);
    GBytes *bytes = soup_session_send_and_read_finish(session, res, NULL);
    if (!bytes) return;

    GError *error = NULL;
    GdkTexture *texture = gdk_texture_new_from_bytes(bytes, &error);
    if (texture) {
        gtk_image_set_from_paintable(GTK_IMAGE(image_widget), GDK_PAINTABLE(texture));
        g_object_unref(texture);
    } else if (error) {
        g_error_free(error);
    }
    g_bytes_unref(bytes);
}

static void do_fetch_image(AppState *state, const char *url) {
    if (!url || url[0] == '\0') return;
    SoupMessage *msg = soup_message_new(SOUP_METHOD_GET, url);
    if (!msg) return;
    soup_session_send_and_read_async(state->http_session, msg,
        G_PRIORITY_DEFAULT, NULL, on_image_ready, state->product_image);
    g_object_unref(msg);
}

/* Called on main thread via g_idle_add */
static gboolean deliver_product(gpointer user_data) {
    FetchCtx *ctx = user_data;
    AppState *state = ctx->state;

    /* If current_product is NULL after fetch → error was already shown */
    if (state->current_product) {
        show_product(state, state->current_product);
        if (state->current_product->image_url &&
            state->current_product->image_url[0] != '\0') {
            do_fetch_image(state, state->current_product->image_url);
        }
    }

    g_free(ctx->barcode);
    g_free(ctx);
    return G_SOURCE_REMOVE;
}

static void on_fetch_done(GObject *source, GAsyncResult *res, gpointer user_data) {
    FetchCtx *ctx = user_data;
    AppState *state = ctx->state;
    SoupSession *session = SOUP_SESSION(source);
    GError *err = NULL;

    GBytes *bytes = soup_session_send_and_read_finish(session, res, &err);
    if (err || !bytes) {
        g_clear_error(&err);
        g_idle_add((GSourceFunc)show_error, state);
        g_free(ctx->barcode);
        g_free(ctx);
        return;
    }

    gsize size = 0;
    const char *data = g_bytes_get_data(bytes, &size);

    JsonParser *parser = json_parser_new();
    if (!json_parser_load_from_data(parser, data, (gssize)size, &err)) {
        g_clear_error(&err);
        g_object_unref(parser);
        g_bytes_unref(bytes);
        state->current_product = NULL;
        g_idle_add(deliver_product, ctx);
        return;
    }

    JsonNode *root_node = json_parser_get_root(parser);
    JsonObject *root = json_node_get_object(root_node);

    /* Check status */
    gint64 status_code = 0;
    if (json_object_has_member(root, "status"))
        status_code = json_object_get_int_member(root, "status");

    if (status_code != 1) {
        state->current_product = NULL;
    } else {
        state->current_product = parse_product_json(root, ctx->barcode);
        /* Add to history */
        if (state->current_product)
            state->history = g_list_prepend(state->history, state->current_product);
    }

    g_object_unref(parser);
    g_bytes_unref(bytes);
    g_idle_add(deliver_product, ctx);
}

void fetch_product(AppState *state, const char *barcode) {
    char *url = g_strdup_printf(OFF_API, barcode);
    SoupMessage *msg = soup_message_new(SOUP_METHOD_GET, url);
    g_free(url);
    if (!msg) return;

    /* Add a user-agent (OpenFoodFacts requirement) */
    SoupMessageHeaders *hdrs = soup_message_get_request_headers(msg);
    soup_message_headers_replace(hdrs, "User-Agent",
        "librefood/1.0 (davide.plozner@etik.com)");

    FetchCtx *ctx = g_new0(FetchCtx, 1);
    ctx->state   = state;
    ctx->barcode = g_strdup(barcode);

    soup_session_send_and_read_async(state->http_session, msg,
        G_PRIORITY_DEFAULT, NULL, on_fetch_done, ctx);
    g_object_unref(msg);
}
