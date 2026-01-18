#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "civetweb.h"
#include "contacts.h"

enum {
    MAX_BODY = 2048,
    MAX_RESPONSE = 65536
};

static const char *http_status_text(int status) {
    switch (status) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

static void send_json(struct mg_connection *conn, int status, const char *body) {
    const char *text = http_status_text(status);
    size_t len = body ? strlen(body) : 0;
    mg_printf(conn,
              "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\n\r\n",
              status, text, len);
    if (body && len > 0) {
        mg_write(conn, body, len);
    }
}

static void send_error(struct mg_connection *conn, int status, const char *message) {
    char body[256];
    snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
    send_json(conn, status, body);
}

static int read_body(struct mg_connection *conn, const struct mg_request_info *ri,
                     char *buf, size_t buf_size) {
    long long length = ri->content_length;
    if (length <= 0 || length >= (long long)buf_size) {
        return -1;
    }
    size_t total = 0;
    while (total < (size_t)length) {
        int nread = mg_read(conn, buf + total, (size_t)length - total);
        if (nread <= 0) {
            return -1;
        }
        total += (size_t)nread;
    }
    buf[total] = '\0';
    return (int)total;
}

static const char *skip_ws(const char *s) {
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

static const char *find_json_key(const char *json, const char *key) {
    char needle[64];
    if (snprintf(needle, sizeof(needle), "\"%s\"", key) >= (int)sizeof(needle)) {
        return NULL;
    }
    const char *pos = strstr(json, needle);
    if (!pos) {
        return NULL;
    }
    pos += strlen(needle);
    pos = strchr(pos, ':');
    if (!pos) {
        return NULL;
    }
    return skip_ws(pos + 1);
}

static int json_get_string(const char *json, const char *key, char *out, size_t out_len) {
    if (!out || out_len == 0) {
        return 0;
    }
    const char *p = find_json_key(json, key);
    if (!p || *p != '"') {
        return 0;
    }
    p++;
    size_t idx = 0;
    while (*p && *p != '"') {
        char c = *p;
        if (c == '\\') {
            p++;
            if (!*p) {
                return 0;
            }
            switch (*p) {
                case '"': c = '"'; break;
                case '\\': c = '\\'; break;
                case '/': c = '/'; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'u': {
                    int value = 0;
                    for (int i = 0; i < 4; i++) {
                        char h = *++p;
                        if (!h) {
                            return 0;
                        }
                        value <<= 4;
                        if (h >= '0' && h <= '9') value |= (h - '0');
                        else if (h >= 'a' && h <= 'f') value |= (h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') value |= (h - 'A' + 10);
                        else return 0;
                    }
                    c = (value <= 0x7F) ? (char)value : '?';
                    break;
                }
                default:
                    return 0;
            }
        }
        if (idx + 1 >= out_len) {
            return 0;
        }
        out[idx++] = c;
        p++;
    }
    if (*p != '"') {
        return 0;
    }
    out[idx] = '\0';
    return 1;
}

static int buf_append(char *buf, size_t *offset, size_t cap, const char *fmt, ...) {
    if (*offset >= cap) {
        return 0;
    }
    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf + *offset, cap - *offset, fmt, args);
    va_end(args);
    if (written < 0 || (size_t)written >= cap - *offset) {
        return 0;
    }
    *offset += (size_t)written;
    return 1;
}

static int buf_append_json_string(char *buf, size_t *offset, size_t cap, const char *value) {
    if (!buf_append(buf, offset, cap, "\"")) {
        return 0;
    }
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') {
            if (!buf_append(buf, offset, cap, "\\%c", c)) {
                return 0;
            }
        } else if (c == '\b') {
            if (!buf_append(buf, offset, cap, "\\b")) return 0;
        } else if (c == '\f') {
            if (!buf_append(buf, offset, cap, "\\f")) return 0;
        } else if (c == '\n') {
            if (!buf_append(buf, offset, cap, "\\n")) return 0;
        } else if (c == '\r') {
            if (!buf_append(buf, offset, cap, "\\r")) return 0;
        } else if (c == '\t') {
            if (!buf_append(buf, offset, cap, "\\t")) return 0;
        } else if (c < 0x20) {
            if (!buf_append(buf, offset, cap, "\\u%04x", c)) return 0;
        } else {
            if (!buf_append(buf, offset, cap, "%c", c)) return 0;
        }
    }
    return buf_append(buf, offset, cap, "\"");
}

static int append_contact_json(char *buf, size_t *offset, size_t cap, const Contact *contact) {
    if (!buf_append(buf, offset, cap, "{\"id\":%d,\"name\":", contact->id)) return 0;
    if (!buf_append_json_string(buf, offset, cap, contact->name)) return 0;
    if (!buf_append(buf, offset, cap, ",\"email\":")) return 0;
    if (!buf_append_json_string(buf, offset, cap, contact->email)) return 0;
    if (!buf_append(buf, offset, cap, ",\"phone\":")) return 0;
    if (!buf_append_json_string(buf, offset, cap, contact->phone)) return 0;
    return buf_append(buf, offset, cap, "}");
}

static int is_contacts_root(const char *uri) {
    return strcmp(uri, "/contacts") == 0 || strcmp(uri, "/contacts/") == 0;
}

static int parse_contact_id(const char *uri, int *out_id) {
    const char *prefix = "/contacts/";
    size_t prefix_len = strlen(prefix);
    if (strncmp(uri, prefix, prefix_len) != 0) {
        return 0;
    }
    const char *id_str = uri + prefix_len;
    if (*id_str == '\0') {
        return 0;
    }
    char *end = NULL;
    long value = strtol(id_str, &end, 10);
    if (!end || *end != '\0') {
        return 0;
    }
    if (value <= 0 || value > INT_MAX) {
        return 0;
    }
    *out_id = (int)value;
    return 1;
}

static int contacts_handler(struct mg_connection *conn, void *cbdata) {
    (void)cbdata;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    const char *method = ri->request_method ? ri->request_method : "";
    const char *uri = ri->local_uri ? ri->local_uri : (ri->request_uri ? ri->request_uri : "");

    int contact_id = 0;
    int has_id = parse_contact_id(uri, &contact_id);
    int is_root = is_contacts_root(uri);

    if (!has_id && !is_root) {
        return 0;
    }

    if (strcmp(method, "GET") == 0 && is_root) {
        char response[MAX_RESPONSE];
        size_t offset = 0;
        if (!buf_append(response, &offset, sizeof(response), "[")) {
            send_error(conn, 500, "Response too large");
            return 500;
        }
        const Contact *all = contacts_all();
        int count = contacts_count();
        for (int i = 0; i < count; i++) {
            if (i > 0 && !buf_append(response, &offset, sizeof(response), ",")) {
                send_error(conn, 500, "Response too large");
                return 500;
            }
            if (!append_contact_json(response, &offset, sizeof(response), &all[i])) {
                send_error(conn, 500, "Response too large");
                return 500;
            }
        }
        if (!buf_append(response, &offset, sizeof(response), "]")) {
            send_error(conn, 500, "Response too large");
            return 500;
        }
        send_json(conn, 200, response);
        return 200;
    }

    if (strcmp(method, "GET") == 0 && has_id) {
        Contact contact;
        if (!contacts_get(contact_id, &contact)) {
            send_error(conn, 404, "Contact not found");
            return 404;
        }
        char response[1024];
        size_t offset = 0;
        if (!append_contact_json(response, &offset, sizeof(response), &contact)) {
            send_error(conn, 500, "Response too large");
            return 500;
        }
        send_json(conn, 200, response);
        return 200;
    }

    if (strcmp(method, "POST") == 0 && is_root) {
        char body[MAX_BODY];
        if (read_body(conn, ri, body, sizeof(body)) < 0) {
            send_error(conn, 400, "Invalid request body");
            return 400;
        }
        Contact input;
        memset(&input, 0, sizeof(input));
        if (!json_get_string(body, "name", input.name, sizeof(input.name)) ||
            !json_get_string(body, "email", input.email, sizeof(input.email)) ||
            !json_get_string(body, "phone", input.phone, sizeof(input.phone))) {
            send_error(conn, 400, "Missing or invalid fields");
            return 400;
        }
        Contact created;
        if (!contacts_create(&input, &created)) {
            send_error(conn, 409, "Contact list is full");
            return 409;
        }
        char response[1024];
        size_t offset = 0;
        if (!append_contact_json(response, &offset, sizeof(response), &created)) {
            send_error(conn, 500, "Response too large");
            return 500;
        }
        send_json(conn, 201, response);
        return 201;
    }

    if (strcmp(method, "PUT") == 0 && has_id) {
        char body[MAX_BODY];
        if (read_body(conn, ri, body, sizeof(body)) < 0) {
            send_error(conn, 400, "Invalid request body");
            return 400;
        }
        Contact input;
        memset(&input, 0, sizeof(input));
        if (!json_get_string(body, "name", input.name, sizeof(input.name)) ||
            !json_get_string(body, "email", input.email, sizeof(input.email)) ||
            !json_get_string(body, "phone", input.phone, sizeof(input.phone))) {
            send_error(conn, 400, "Missing or invalid fields");
            return 400;
        }
        Contact updated;
        if (!contacts_update(contact_id, &input, &updated)) {
            send_error(conn, 404, "Contact not found");
            return 404;
        }
        char response[1024];
        size_t offset = 0;
        if (!append_contact_json(response, &offset, sizeof(response), &updated)) {
            send_error(conn, 500, "Response too large");
            return 500;
        }
        send_json(conn, 200, response);
        return 200;
    }

    if (strcmp(method, "DELETE") == 0 && has_id) {
        if (!contacts_delete(contact_id)) {
            send_error(conn, 404, "Contact not found");
            return 404;
        }
        send_json(conn, 204, "");
        return 204;
    }

    send_error(conn, 405, "Method not allowed");
    return 405;
}

int main(void) {
    const char *options[] = {"listening_ports", "8000", "num_threads", "1", NULL};
    struct mg_callbacks callbacks;
    memset(&callbacks, 0, sizeof(callbacks));

    struct mg_context *ctx = mg_start(&callbacks, NULL, options);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to start CivetWeb server\n");
        return 1;
    }

    mg_set_request_handler(ctx, "/contacts", contacts_handler, NULL);
    mg_set_request_handler(ctx, "/contacts/", contacts_handler, NULL);

    printf("Starting REST API server on http://localhost:8000\n");
    printf("Try: curl http://localhost:8000/contacts\n");

    for (;;) {
        #ifdef _WIN32
        Sleep(1000);
        #else
        struct timespec ts;
        ts.tv_sec = 1;
        ts.tv_nsec = 0;
        nanosleep(&ts, NULL);
        #endif
    }

    mg_stop(ctx);
    return 0;
}
