#include <stdio.h>
#include <libwebsockets.h>
#include <jansson.h>
#include <sqlite3.h>

#include "config.h"

typedef struct
{
    const char *message;
    int id;
    const char *type;
    const char *stack_trace;
} Data;

void send_command(struct lws *wsi, const char *msg, int identifier)
{
    if (wsi == NULL)
    {
        perror("ERROR: Connection is NULL\n");
        return;
    }

    char packet[1024];
    snprintf(packet, sizeof(packet), "{\"Identifier\": %d, \"Message\": \"%s\", \"Name\": \"WebRcon\"}", identifier, msg);

    unsigned char buf[LWS_PRE + 1024];
    unsigned char *p = &buf[LWS_PRE];

    size_t n = sprintf((char *)p, "%s", packet);

    lws_write(wsi, p, n, LWS_WRITE_TEXT);
}

static int callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    switch (reason)
    {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        printf("INFO: Connection established\n");

        send_command(wsi, "status", 999);
        break;
    case LWS_CALLBACK_CLIENT_RECEIVE:
    {
        json_error_t error;
        json_t *root;

        root = json_loads((const char *)in, 0, &error);

        if (!root)
        {
            perror("ERROR: Failed to parse JSON\n");
            return 1;
        }

        Data data = {NULL, -1, NULL, NULL};
        json_t *message_json, *identifier_json, *type_json, *stacktrace_json;

        message_json = json_object_get(root, "Message");
        identifier_json = json_object_get(root, "Identifier");
        type_json = json_object_get(root, "Type");
        stacktrace_json = json_object_get(root, "StackTrace");

        if (json_is_string(message_json))
        {
            data.message = json_string_value(message_json);
        }

        if (json_is_integer(identifier_json))
        {
            data.id = json_integer_value(identifier_json);
        }

        if (json_is_string(type_json))
        {
            data.type = json_string_value(type_json);
        }

        if (json_is_string(stacktrace_json))
        {
            data.stack_trace = json_string_value(stacktrace_json);
        }

        if (data.message != NULL)
        {
            printf("INFO: Received message:\n");
            printf("\tWith identifier:  %d\n", data.id);
            printf("\tWith type:        %s\n\n\n", data.type);
            printf("%s\n", data.message);

            if (data.id == 999)
            {
                break;
            }

            sqlite3 *db;
            char *err_msg = 0;

            int rc = sqlite3_open("/var/lib/rrccon/rrccon.db", &db);

            if (rc != SQLITE_OK)
            {
                perror("ERROR: Failed to open database\n");
                sqlite3_close(db);

                exit(1);
            }

            char *sql = "CREATE TABLE IF NOT EXISTS logs("
                        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "message TEXT NOT NULL,"
                        "type TEXT NOT NULL,"
                        "stack_trace TEXT"
                        ");";

            rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

            if (rc != SQLITE_OK)
            {
                perror("ERROR: Failed to create table\n");
                sqlite3_free(err_msg);
                sqlite3_close(db);

                exit(1);
            }

            sql = sqlite3_mprintf("INSERT INTO logs (message, type, stack_trace) VALUES ('%q', '%q', '%q');", data.message, data.type, data.stack_trace);

            rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

            if (rc != SQLITE_OK)
            {
                perror("ERROR: Failed to insert data\n");
                sqlite3_free(err_msg);
                sqlite3_close(db);

                exit(1);
            }

            sqlite3_close(db);
        }

        json_decref(root);
    }
    break;
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        printf("INFO: Connection error\n");
        break;
    default:
        break;
    }

    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "rrccon",
        callback,
        0,
        0,
    },
    {NULL, NULL, 0, 0}};

int main(void)
{
    if (geteuid() != 0)
    {
        perror("ERROR: You must be root to run this program\n");
        return 1;
    }

    struct lws_context_creation_info info;
    struct lws_client_connect_info ccinfo;
    struct lws_context *context;
    struct lws *wsi;

    lws_set_log_level(0, NULL);

    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    context = lws_create_context(&info);

    if (context == NULL)
    {
        perror("ERROR: Failed creating context\n");
        return 1;
    }

    memset(&ccinfo, 0, sizeof(ccinfo));
    ccinfo.context = context;
    ccinfo.address = ADDRESS;
    ccinfo.port = PORT;
    ccinfo.path = PASSWORD;
    ccinfo.host = lws_canonical_hostname(context);
    ccinfo.origin = "origin";
    ccinfo.protocol = protocols[0].name;

    wsi = lws_client_connect_via_info(&ccinfo);

    if (wsi == NULL)
    {
        perror("ERROR: Failed creating connection\n");
        return 1;
    }

    while (1)
    {
        lws_service(context, 500);
    }

    lws_context_destroy(context);

    return 0;
}