#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "contacts.h"

enum {
    SERVER_PORT = 8000,
    MAX_REQUEST = 16384,
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
        case 413: return "Payload Too Large";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

static int send_all(int fd, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t sent = send(fd, buf + total, len - total, 0);
        if (sent <= 0) {
            return 0;
        }
        total += (size_t)sent;
    }
    return 1;
}

static void send_json(int fd, int status, const char *body) {
    const char *text = http_status_text(status);
    size_t len = body ? strlen(body) : 0;
    char header[256];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 %d %s\r\n"
                              "Content-Type: application/json\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n\r\n",
                              status, text, len);
    if (header_len > 0) {
        send_all(fd, header, (size_t)header_len);
    }
    if (body && len > 0) {
        send_all(fd, body, len);
    }
}

static void send_error(int fd, int status, const char *message) {
    char body[256];
    snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
    send_json(fd, status, body);
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
            if (!buf_append(buf, offset, cap, "\\%c", c)) return 0;
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
                        if (!h) return 0;
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

static int is_contacts_root(const char *path) {
    return strcmp(path, "/contacts") == 0 || strcmp(path, "/contacts/") == 0;
}

static int parse_contact_id(const char *path, int *out_id) {
    const char *prefix = "/contacts/";
    size_t prefix_len = strlen(prefix);
    if (strncmp(path, prefix, prefix_len) != 0) {
        return 0;
    }
    const char *id_str = path + prefix_len;
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

static int parse_content_length(const char *headers) {
    const char *pos = headers;
    while (pos && *pos) {
        const char *line_end = strstr(pos, "\r\n");
        size_t line_len = line_end ? (size_t)(line_end - pos) : strlen(pos);
        if (line_len >= 15 && strncasecmp(pos, "Content-Length:", 15) == 0) {
            const char *value = pos + 15;
            while (*value == ' ' || *value == '\t') value++;
            long len = strtol(value, NULL, 10);
            if (len > INT_MAX) {
                return -1;
            }
            return (int)len;
        }
        if (!line_end) {
            break;
        }
        pos = line_end + 2;
    }
    return 0;
}

static int read_request(int fd, char *buffer, size_t buffer_size,
                        char **out_body, int *out_body_len) {
    size_t total = 0;
    buffer[0] = '\0';
    while (total + 1 < buffer_size) {
        ssize_t nread = recv(fd, buffer + total, buffer_size - total - 1, 0);
        if (nread <= 0) {
            return 0;
        }
        total += (size_t)nread;
        buffer[total] = '\0';
        if (strstr(buffer, "\r\n\r\n")) {
            break;
        }
    }

    char *header_end = strstr(buffer, "\r\n\r\n");
    if (!header_end) {
        return 0;
    }
    *header_end = '\0';
    char *body_start = header_end + 4;
    int body_len = parse_content_length(buffer);
    if (body_len < 0) {
        return 0;
    }
    size_t have_body = total - (size_t)(body_start - buffer);
    if (body_len > 0) {
        if (body_len >= (int)(buffer_size - 1)) {
            return -1;
        }
        while ((int)have_body < body_len) {
            ssize_t nread = recv(fd, buffer + total, buffer_size - total - 1, 0);
            if (nread <= 0) {
                return 0;
            }
            total += (size_t)nread;
            buffer[total] = '\0';
            have_body = total - (size_t)(body_start - buffer);
        }
        body_start[body_len] = '\0';
    }

    *out_body = body_start;
    *out_body_len = body_len;
    return 1;
}

static void handle_request(int client_fd, const char *method, const char *path,
                           const char *body, int body_len) {
    int contact_id = 0;
    int has_id = parse_contact_id(path, &contact_id);
    int is_root = is_contacts_root(path);

    if (!has_id && !is_root) {
        send_error(client_fd, 404, "Not found");
        return;
    }

    if (strcmp(method, "GET") == 0 && is_root) {
        char response[MAX_RESPONSE];
        size_t offset = 0;
        if (!buf_append(response, &offset, sizeof(response), "[")) {
            send_error(client_fd, 500, "Response too large");
            return;
        }
        const Contact *all = contacts_all();
        int count = contacts_count();
        for (int i = 0; i < count; i++) {
            if (i > 0 && !buf_append(response, &offset, sizeof(response), ",")) {
                send_error(client_fd, 500, "Response too large");
                return;
            }
            if (!append_contact_json(response, &offset, sizeof(response), &all[i])) {
                send_error(client_fd, 500, "Response too large");
                return;
            }
        }
        if (!buf_append(response, &offset, sizeof(response), "]")) {
            send_error(client_fd, 500, "Response too large");
            return;
        }
        send_json(client_fd, 200, response);
        return;
    }

    if (strcmp(method, "GET") == 0 && has_id) {
        Contact contact;
        if (!contacts_get(contact_id, &contact)) {
            send_error(client_fd, 404, "Contact not found");
            return;
        }
        char response[1024];
        size_t offset = 0;
        if (!append_contact_json(response, &offset, sizeof(response), &contact)) {
            send_error(client_fd, 500, "Response too large");
            return;
        }
        send_json(client_fd, 200, response);
        return;
    }

    if ((strcmp(method, "POST") == 0 && is_root) ||
        (strcmp(method, "PUT") == 0 && has_id)) {
        if (body_len <= 0 || body_len >= MAX_BODY) {
            send_error(client_fd, 400, "Invalid request body");
            return;
        }
        Contact input;
        memset(&input, 0, sizeof(input));
        if (!json_get_string(body, "name", input.name, sizeof(input.name)) ||
            !json_get_string(body, "email", input.email, sizeof(input.email)) ||
            !json_get_string(body, "phone", input.phone, sizeof(input.phone))) {
            send_error(client_fd, 400, "Missing or invalid fields");
            return;
        }

        if (strcmp(method, "POST") == 0) {
            Contact created;
            if (!contacts_create(&input, &created)) {
                send_error(client_fd, 409, "Contact list is full");
                return;
            }
            char response[1024];
            size_t offset = 0;
            if (!append_contact_json(response, &offset, sizeof(response), &created)) {
                send_error(client_fd, 500, "Response too large");
                return;
            }
            send_json(client_fd, 201, response);
            return;
        }

        Contact updated;
        if (!contacts_update(contact_id, &input, &updated)) {
            send_error(client_fd, 404, "Contact not found");
            return;
        }
        char response[1024];
        size_t offset = 0;
        if (!append_contact_json(response, &offset, sizeof(response), &updated)) {
            send_error(client_fd, 500, "Response too large");
            return;
        }
        send_json(client_fd, 200, response);
        return;
    }

    if (strcmp(method, "DELETE") == 0 && has_id) {
        if (!contacts_delete(contact_id)) {
            send_error(client_fd, 404, "Contact not found");
            return;
        }
        send_json(client_fd, 204, "");
        return;
    }

    send_error(client_fd, 405, "Method not allowed");
}

static void handle_client(int client_fd) {
    char buffer[MAX_REQUEST];
    char *body = NULL;
    int body_len = 0;
    int read_status = read_request(client_fd, buffer, sizeof(buffer), &body, &body_len);
    if (read_status == -1) {
        send_error(client_fd, 413, "Payload too large");
        return;
    }
    if (read_status == 0) {
        send_error(client_fd, 400, "Invalid request");
        return;
    }

    char method[8];
    char path[256];
    if (sscanf(buffer, "%7s %255s", method, path) != 2) {
        send_error(client_fd, 400, "Invalid request line");
        return;
    }

    handle_request(client_fd, method, path, body, body_len);
}

int main(void) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Starting REST API server on http://localhost:%d\n", SERVER_PORT);
    printf("Try: curl http://localhost:%d/contacts\n", SERVER_PORT);

    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("accept");
            break;
        }
        handle_client(client_fd);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
