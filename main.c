// Copyright (c) 2015 Cesanta Software Limited
// All rights reserved

#include "mongoose.h"

#define EP_UPLOAD "/upload"
#define UPLOAD_DIR "./uploaded_files"

struct file_writer_data
{
    FILE *fp;
    size_t bytes_written;
    bool should_write;
};
static const char *s_http_port = "3000";
static struct mg_serve_http_opts s_http_server_opts;

static void handle_upload(struct mg_connection *nc, int ev, void *p)
{
    struct file_writer_data *data = (struct file_writer_data *)nc->user_data;

    // only available for MG_EV_HTTP_MULTIPART_REQUEST
    struct http_message *hm = (struct http_message *)p;
    // only available for the rest of cases
    struct mg_http_multipart_part *mp = (struct mg_http_multipart_part *)p;

    switch (ev)
    {
    case MG_EV_HTTP_MULTIPART_REQUEST:
        // the initial request. Will not occur again for each part

        printf("The query string to the upload endpoint: %.*s\n", hm->query_string.len, hm->query_string.p);

        break;

    case MG_EV_HTTP_PART_BEGIN:
    {
        if (data == NULL)
        {
            // data needs to be allocated anyway. It seems we have no way to stop the upload. just use should write to ignore the bad data.
            data = calloc(1, sizeof(struct file_writer_data));
            data->bytes_written = 0;
            data->should_write = true;
            nc->user_data = (void *)data;

            char dest_path[MAXPATHLEN];
            if (mp->file_name == NULL || strlen(mp->file_name) == 0)
            {
                // be cautious that mg_str.p does not end with 0!
                printf("The posted data contains the non-file object: '%s'. Skip.\n", mp->var_name);
                data->should_write = false;
                return;
            }
            snprintf(dest_path, MAXPATHLEN, "%s/%s", UPLOAD_DIR, mp->file_name);

            data->fp = fopen(dest_path, "wb");

            if (data->fp == NULL)
            {

                mg_printf(nc, "%s",
                          "HTTP/1.1 500 Failed to open a file\r\n"
                          "Content-Length: 0\r\n\r\n");
                nc->flags |= MG_F_SEND_AND_CLOSE;
                free(data);
                return;
            }
        }
        break;
    }
    case MG_EV_HTTP_PART_DATA:
    {

        if (!data->should_write)
            return;

        if (fwrite(mp->data.p, 1, mp->data.len, data->fp) != mp->data.len)
        {
            mg_printf(nc, "%s",
                      "HTTP/1.1 500 Failed to write to a file\r\n"
                      "Content-Length: 0\r\n\r\n");
            nc->flags |= MG_F_SEND_AND_CLOSE;
            return;
        }

        data->bytes_written += mp->data.len;
        break;
    }
    case MG_EV_HTTP_PART_END:
    {

        if (data->should_write)
        {
            // fxxk segmentation fault
            mg_printf(nc,
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain\r\n"
                      "Connection: close\r\n\r\n"
                      "Written %ld of POST data to a temp file\n\n",
                      (long)ftell(data->fp));
            fclose(data->fp);
        }
        nc->flags |= MG_F_SEND_AND_CLOSE;

        free(data);
        nc->user_data = NULL;

        break;
    }
    }
}

static void ev_handler(struct mg_connection *nc, int ev, void *ev_data)
{
    struct http_message *hm = (struct http_message *)ev_data;
    char path_buffer[MG_MAX_PATH];
    struct mg_str path = mg_mk_str_n(path_buffer, MG_MAX_PATH);

    switch (ev)
    {
    case MG_EV_HTTP_REQUEST:
        mg_serve_http(nc, hm, s_http_server_opts); /* Serve static content */

    default:
        break;
    }
}

int main(void)
{
    struct mg_mgr mgr;
    struct mg_connection *nc;

    mg_mgr_init(&mgr, NULL);
    printf("Starting web server on port %s\n", s_http_port);
    nc = mg_bind(&mgr, s_http_port, ev_handler);
    if (nc == NULL)
    {
        printf("Failed to create listener\n");
        return 1;
    }
    mg_register_http_endpoint(nc, "/upload", handle_upload MG_UD_ARG(NULL));
    // Set up HTTP server parameters
    mg_set_protocol_http_websocket(nc);
    s_http_server_opts.document_root = "./static"; // Serve current directory
    s_http_server_opts.enable_directory_listing = "yes";

    for (;;)
    {
        mg_mgr_poll(&mgr, 1000);
    }
    mg_mgr_free(&mgr);

    return 0;
}