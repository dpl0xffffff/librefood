#include "librefood.h"

#define OFF_BASE_URL "https://world.openfoodfacts.org"

/*
 * All libsoup async callbacks run on the main thread (the thread-default
 * main context of the caller), so UI can be touched directly from them.
 *
 * Every request carries its own GCancellable. Starting a new request
 * cancels the previous one, and a callback whose cancellable is no longer
 * the current one is stale and must not touch the UI.
 */

typedef struct {
    AppState     *state;
    SoupMessage  *msg;
    GCancellable *cancel;
    char         *barcode;
} FetchCtx;

static void fetch_ctx_free(FetchCtx *ctx) {
    g_clear_object(&ctx->msg);
    g_clear_object(&ctx->cancel);
    g_free(ctx->barcode);
    g_free(ctx);
}

static void replace_cancellable(GCancellable **slot) {
    if (*slot) {
        g_cancellable_cancel(*slot);
        g_object_unref(*slot);
    }
    *slot = g_cancellable_new();
}

void cancel_network(AppState *state) {
    if (state->fetch_cancel) {
        g_cancellable_cancel(state->fetch_cancel);
        g_clear_object(&state->fetch_cancel);
    }
    if (state->image_cancel) {
        g_cancellable_cancel(state->image_cancel);
        g_clear_object(&state->image_cancel);
    }
}

/* ── Product image ── */

static void on_image_ready(GObject *source, GAsyncResult *res, gpointer user_data) {
    GCancellable *cancel = user_data;          /* owned reference */
    GError *error = NULL;
    GBytes *bytes = soup_session_send_and_read_finish(SOUP_SESSION(source), res, &error);

    if (g_cancellable_is_cancelled(cancel)) {
        /* Superseded by a newer product view: drop silently. */
        goto out;
    }
    AppState *state = g_object_get_data(G_OBJECT(cancel), "state");
    if (!state) goto out;
    if (state->image_cancel == cancel)
        g_clear_object(&state->image_cancel);

    if (!bytes) {
        /* Non-essential: keep the placeholder icon, but leave a trace. */
        g_warning("librefood: image download failed: %s",
                  error ? error->message : "unknown error");
        goto out;
    }

    GdkTexture *texture = gdk_texture_new_from_bytes(bytes, &error);
    if (texture) {
        gtk_image_set_from_paintable(GTK_IMAGE(state->product_image),
                                     GDK_PAINTABLE(texture));
        g_object_unref(texture);
    } else {
        g_warning("librefood: could not decode product image: %s",
                  error ? error->message : "unknown error");
    }

out:
    g_clear_error(&error);
    if (bytes) g_bytes_unref(bytes);
    g_object_unref(cancel);
}

/* Cancels any pending image download, then starts one for url (if any). */
void fetch_image(AppState *state, const char *url) {
    if (state->image_cancel) {
        g_cancellable_cancel(state->image_cancel);
        g_clear_object(&state->image_cancel);
    }
    if (!url || !(g_str_has_prefix(url, "https://") ||
                  g_str_has_prefix(url, "http://")))
        return;

    SoupMessage *msg = soup_message_new(SOUP_METHOD_GET, url);
    if (!msg) {
        g_warning("librefood: invalid image URL ignored");
        return;
    }
    state->image_cancel = g_cancellable_new();
    g_object_set_data(G_OBJECT(state->image_cancel), "state", state);
    soup_session_send_and_read_async(state->http_session, msg,
        G_PRIORITY_LOW, state->image_cancel, on_image_ready,
        g_object_ref(state->image_cancel));
    g_object_unref(msg);
}

/* ── Product lookup ── */

static void on_fetch_done(GObject *source, GAsyncResult *res, gpointer user_data) {
    FetchCtx *ctx = user_data;
    AppState *state = ctx->state;
    GError *err = NULL;

    GBytes *bytes = soup_session_send_and_read_finish(SOUP_SESSION(source), res, &err);

    /* Stale or cancelled lookup: a newer one owns the UI now. */
    if (g_cancellable_is_cancelled(ctx->cancel) || state->fetch_cancel != ctx->cancel) {
        g_clear_error(&err);
        if (bytes) g_bytes_unref(bytes);
        fetch_ctx_free(ctx);
        return;
    }
    g_clear_object(&state->fetch_cancel);

    char *error_msg = NULL;
    Product *product = NULL;
    JsonParser *parser = NULL;
    guint http_status = soup_message_get_status(ctx->msg);

    if (!bytes) {
        error_msg = g_strdup_printf(
            "Could not reach OpenFoodFacts: %s\nCheck your connection and try again.",
            err ? err->message : "unknown network error");
        goto done;
    }

    /* OFF answers unknown products with 200 + status 0 (v0) or 404 (v2);
     * any other non-2xx is a server-side problem (rate limit, outage…). */
    if (http_status != 404 && !SOUP_STATUS_IS_SUCCESSFUL(http_status)) {
        error_msg = g_strdup_printf(
            "OpenFoodFacts returned an error (HTTP %u %s). Please try again later.",
            http_status, soup_status_get_phrase(http_status)
                         ? soup_status_get_phrase(http_status) : "");
        goto done;
    }

    gsize size = 0;
    const char *data = g_bytes_get_data(bytes, &size);
    parser = json_parser_new();
    if (!data || size == 0 ||
        !json_parser_load_from_data(parser, data, (gssize)size, &err)) {
        g_warning("librefood: invalid JSON from OFF: %s",
                  err ? err->message : "empty response");
        error_msg = g_strdup("OpenFoodFacts sent an unreadable response. Please try again.");
        goto done;
    }

    JsonNode *root_node = json_parser_get_root(parser);
    JsonObject *root = (root_node && JSON_NODE_HOLDS_OBJECT(root_node))
                       ? json_node_get_object(root_node) : NULL;
    product = parse_product_json(root, ctx->barcode);
    if (!product) {
        error_msg = g_strdup_printf(
            "Product %s was not found in the OpenFoodFacts database.", ctx->barcode);
    }

done:
    if (product) {
        history_add(state, product);       /* takes ownership */
        show_product(state, product);      /* also starts image download */
    } else {
        show_error(state, error_msg);
    }
    g_free(error_msg);
    g_clear_error(&err);
    g_clear_object(&parser);
    if (bytes) g_bytes_unref(bytes);
    fetch_ctx_free(ctx);
}

void fetch_product(AppState *state, const char *barcode) {
    /* barcode is validated digits-only by the caller (barcode_normalize),
     * so it is safe to splice into the URL. */
    /* LIBREFOOD_API_URL overrides the server base (used by the tests). It is
     * concatenated, never used as a format string. */
    const char *base = g_getenv("LIBREFOOD_API_URL");
    if (!base || !*base) base = OFF_BASE_URL;
    char *url = g_strconcat(base, "/api/v0/product/", barcode, ".json", NULL);
    SoupMessage *msg = soup_message_new(SOUP_METHOD_GET, url);
    g_free(url);
    if (!msg) {
        show_error(state, "Invalid barcode.");
        return;
    }

    FetchCtx *ctx = g_new0(FetchCtx, 1);
    ctx->state   = state;
    ctx->msg     = msg;                       /* keep for HTTP status */
    ctx->barcode = g_strdup(barcode);

    replace_cancellable(&state->fetch_cancel); /* supersede older lookup */
    ctx->cancel = g_object_ref(state->fetch_cancel);

    soup_session_send_and_read_async(state->http_session, msg,
        G_PRIORITY_DEFAULT, ctx->cancel, on_fetch_done, ctx);
}
