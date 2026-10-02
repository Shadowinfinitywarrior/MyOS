#include "http.h"
#include "tcp.h"
#include "socket.h"
#include "dns.h"
#include "../lib/string.h"
#include "../lib/printf.h"
#include "byteorder.h"

static int (*g_http_handler)(const http_request_t *req, http_response_t *resp, char *body_buf, int body_size) = NULL;

int http_parse_request(const char *buf, int len, http_request_t *req) {
    if (!buf || !req || len <= 0) return -1;
    memset(req, 0, sizeof(http_request_t));
    
    // Parse request line
    const char *p = buf;
    const char *end = buf + len;
    const char *line_end = strstr(p, "\r\n");
    if (!line_end) line_end = strstr(p, "\n");
    if (!line_end) return -1;
    
    // Method
    const char *space1 = strchr(p, ' ');
    if (!space1 || space1 >= line_end) return -1;
    int mlen = space1 - p;
    if (mlen >= (int)sizeof(req->method)) mlen = sizeof(req->method) - 1;
    memcpy(req->method, p, mlen);
    req->method[mlen] = '\0';
    p = space1 + 1;
    
    // Path
    const char *space2 = strchr(p, ' ');
    if (!space2 || space2 >= line_end) return -1;
    int plen = space2 - p;
    if (plen >= (int)sizeof(req->path)) plen = sizeof(req->path) - 1;
    memcpy(req->path, p, plen);
    req->path[plen] = '\0';
    p = space2 + 1;
    
    // Version
    int vlen = line_end - p;
    if (vlen > 0 && p[vlen-1] == '\r') vlen--;
    if (vlen >= (int)sizeof(req->version)) vlen = sizeof(req->version) - 1;
    memcpy(req->version, p, vlen);
    req->version[vlen] = '\0';
    
    // Parse headers
    p = line_end;
    if (p < end && *p == '\r') p++;
    if (p < end && *p == '\n') p++;
    
    while (p < end - 1) {
        const char *h_end = strstr(p, "\r\n");
        if (!h_end) h_end = strstr(p, "\n");
        if (!h_end) break;
        if (h_end == p) break; // empty line
        const char *colon = strchr(p, ':');
        if (colon && colon < h_end) {
            const char *hval = colon + 1;
            while (hval < h_end && (*hval == ' ' || *hval == '\t')) hval++;
            int hval_len = h_end - hval;
            if (hval_len > 0 && *(hval + hval_len - 1) == '\r') hval_len--;
            
            if (strncasecmp(p, "Host", 4) == 0) {
                if (hval_len >= (int)sizeof(req->host)) hval_len = sizeof(req->host) - 1;
                memcpy(req->host, hval, hval_len);
                req->host[hval_len] = '\0';
            } else if (strncasecmp(p, "Content-Length", 14) == 0) {
                req->content_length = atoi(hval);
            } else if (strncasecmp(p, "Content-Type", 12) == 0) {
                if (hval_len >= (int)sizeof(req->content_type)) hval_len = sizeof(req->content_type) - 1;
                memcpy(req->content_type, hval, hval_len);
                req->content_type[hval_len] = '\0';
            } else if (strncasecmp(p, "Connection", 10) == 0) {
                if (strncasecmp(hval, "keep-alive", 10) == 0) req->keep_alive = 1;
            } else if (strncasecmp(p, "User-Agent", 10) == 0) {
                if (hval_len >= (int)sizeof(req->user_agent)) hval_len = sizeof(req->user_agent) - 1;
                memcpy(req->user_agent, hval, hval_len);
                req->user_agent[hval_len] = '\0';
            }
        }
        p = h_end;
        if (p < end && *p == '\r') p++;
        if (p < end && *p == '\n') p++;
    }
    return 0;
}

int http_find_body(const char *buf, int len) {
    if (!buf || len <= 0) return -1;
    const char *p = strstr(buf, "\r\n\r\n");
    if (p) return (int)(p - buf) + 4;
    /* Tolerate servers that use bare LF line endings. */
    p = strstr(buf, "\n\n");
    if (p) return (int)(p - buf) + 2;
    return -1;
}

/* True once the buffered bytes hold the headers plus the full declared body.
 * Responses without Content-Length (or with -1) are treated as complete once
 * the headers alone have arrived, which matches a bodyless reply such as 204. */
static int http_body_complete(const char *buf, int len) {
    int header_len = http_find_body(buf, len);
    if (header_len < 0) return 0;
    http_response_t meta;
    if (http_parse_response(buf, len, &meta) != 0) return 0;
    if (meta.content_length <= 0) return 1;
    return len >= header_len + meta.content_length;
}

int http_parse_response(const char *buf, int len, http_response_t *resp) {
    if (!buf || !resp || len <= 0) return -1;
    memset(resp, 0, sizeof(http_response_t));
    const char *p = buf;
    const char *end = buf + len;
    const char *line_end = strstr(p, "\r\n");
    if (!line_end) line_end = strstr(p, "\n");
    if (!line_end) return -1;
    const char *space1 = strchr(p, ' ');
    if (!space1 || space1 >= line_end) return -1;
    space1++;
    const char *space2 = strchr(space1, ' ');
    if (space2 && space2 < line_end) {
        int code_len = space2 - space1;
        resp->status_code = 0;
        for (int i = 0; i < code_len; i++) {
            char c = space1[i];
            if (c >= '0' && c <= '9') resp->status_code = resp->status_code * 10 + (c - '0');
        }
        int text_len = line_end - space2 - 1;
        if (text_len > 0 && *(space2 + text_len) == '\r') text_len--;
        if (text_len >= (int)sizeof(resp->status_text)) text_len = sizeof(resp->status_text) - 1;
        memcpy(resp->status_text, space2 + 1, text_len);
        resp->status_text[text_len] = '\0';
    }
    p = line_end;
    if (p < end && *p == '\r') p++;
    if (p < end && *p == '\n') p++;
    while (p < end - 1) {
        const char *h_end = strstr(p, "\r\n");
        if (!h_end) h_end = strstr(p, "\n");
        if (!h_end) break;
        if (h_end == p) break;
        const char *colon = strchr(p, ':');
        if (colon && colon < h_end) {
            const char *hval = colon + 1;
            while (hval < h_end && (*hval == ' ' || *hval == '\t')) hval++;
            int hval_len = h_end - hval;
            if (hval_len > 0 && *(hval + hval_len - 1) == '\r') hval_len--;
            if (strncasecmp(p, "Content-Length", 14) == 0) {
                resp->content_length = atoi(hval);
            } else if (strncasecmp(p, "Content-Type", 12) == 0) {
                if (hval_len >= (int)sizeof(resp->content_type)) hval_len = sizeof(resp->content_type) - 1;
                memcpy(resp->content_type, hval, hval_len);
                resp->content_type[hval_len] = '\0';
            } else if (strncasecmp(p, "Location", 8) == 0) {
                if (hval_len >= (int)sizeof(resp->location)) hval_len = sizeof(resp->location) - 1;
                memcpy(resp->location, hval, hval_len);
                resp->location[hval_len] = '\0';
            } else if (strncasecmp(p, "Connection", 10) == 0) {
                if (strncasecmp(hval, "keep-alive", 10) == 0) resp->keep_alive = 1;
            }
        }
        p = h_end;
        if (p < end && *p == '\r') p++;
        if (p < end && *p == '\n') p++;
    }
    return 0;
}

int http_build_request(http_request_t *req, char *buf, int buf_size) {
    if (!req || !buf || buf_size <= 0) return -1;
    char tmp[512];
    int pos = 0;
    pos += snprintf(tmp + pos, sizeof(tmp) - pos, "%s %s %s\r\n", req->method, req->path, req->version);
    if (req->host[0]) pos += snprintf(tmp + pos, sizeof(tmp) - pos, "Host: %s\r\n", req->host);
    if (req->user_agent[0]) pos += snprintf(tmp + pos, sizeof(tmp) - pos, "User-Agent: %s\r\n", req->user_agent);
    else pos += snprintf(tmp + pos, sizeof(tmp) - pos, "User-Agent: MyOS/1.0\r\n");
    if (req->content_type[0]) pos += snprintf(tmp + pos, sizeof(tmp) - pos, "Content-Type: %s\r\n", req->content_type);
    if (req->content_length > 0) pos += snprintf(tmp + pos, sizeof(tmp) - pos, "Content-Length: %d\r\n", req->content_length);
    pos += snprintf(tmp + pos, sizeof(tmp) - pos, "Connection: %s\r\n\r\n", req->keep_alive ? "keep-alive" : "close");
    if (pos >= buf_size) return -1;
    memcpy(buf, tmp, pos);
    buf[pos] = '\0';
    return pos;
}

int http_build_response(http_response_t *resp, const char *body, char *buf, int buf_size) {
    if (!resp || !buf || buf_size <= 0) return -1;
    char tmp[4096];
    int pos = 0;
    pos += snprintf(tmp + pos, sizeof(tmp) - pos, "HTTP/1.1 %d %s\r\n", resp->status_code, resp->status_text);
    if (resp->content_type[0]) pos += snprintf(tmp + pos, sizeof(tmp) - pos, "Content-Type: %s\r\n", resp->content_type);
    else pos += snprintf(tmp + pos, sizeof(tmp) - pos, "Content-Type: text/plain\r\n");
    int body_len = body ? strlen(body) : 0;
    if (resp->content_length >= 0) pos += snprintf(tmp + pos, sizeof(tmp) - pos, "Content-Length: %d\r\n", resp->content_length > 0 ? resp->content_length : body_len);
    pos += snprintf(tmp + pos, sizeof(tmp) - pos, "Connection: %s\r\n\r\n", resp->keep_alive ? "keep-alive" : "close");
    if (body && body_len > 0 && pos + body_len < (int)sizeof(tmp)) {
        memcpy(tmp + pos, body, body_len);
        pos += body_len;
    }
    if (pos >= buf_size) return -1;
    memcpy(buf, tmp, pos);
    return pos;
}

int http_client_get(const char *host, uint32_t ip, int port, const char *path, char *resp_buf, int resp_size) {
    if (!host || !resp_buf || resp_size <= 0) return -1;
    int sock = tcp_socket();
    if (sock < 0) return -1;
    if (tcp_connect(sock, htonl(ip), htons((uint16_t)port)) < 0) {
        tcp_close(sock);
        return -1;
    }
    http_request_t req;
    memset(&req, 0, sizeof(req));
    strcpy(req.method, "GET");
    if (path) strcpy(req.path, path);
    else strcpy(req.path, "/");
    strcpy(req.version, "HTTP/1.1");
    strcpy(req.host, host);
    strcpy(req.user_agent, "MyOS/1.0");
    req.keep_alive = 0;
    char req_buf[512];
    int req_len = http_build_request(&req, req_buf, sizeof(req_buf));
    if (req_len < 0) {
        tcp_close(sock);
        return -1;
    }
    tcp_send(sock, (uint8_t *)req_buf, req_len);
    int total = 0;
    while (total < resp_size - 1) {
        int n = tcp_recv(sock, (uint8_t *)(resp_buf + total), resp_size - 1 - total);
        if (n <= 0) break;
        total += n;
        /* Keep reading until the declared body is complete, not just the
         * headers: header_len + Content-Length is the full message size. */
        if (http_body_complete(resp_buf, total)) break;
    }
    resp_buf[total] = '\0';
    tcp_close(sock);
    return total;
}

int http_client_post(const char *host, uint32_t ip, int port, const char *path, const char *body, const char *content_type, char *resp_buf, int resp_size) {
    if (!host || !resp_buf || resp_size <= 0) return -1;
    int sock = tcp_socket();
    if (sock < 0) return -1;
    if (tcp_connect(sock, htonl(ip), htons((uint16_t)port)) < 0) {
        tcp_close(sock);
        return -1;
    }
    http_request_t req;
    memset(&req, 0, sizeof(req));
    strcpy(req.method, "POST");
    if (path) strcpy(req.path, path);
    else strcpy(req.path, "/");
    strcpy(req.version, "HTTP/1.1");
    strcpy(req.host, host);
    strcpy(req.user_agent, "MyOS/1.0");
    if (content_type) strcpy(req.content_type, content_type);
    else strcpy(req.content_type, "text/plain");
    req.content_length = body ? strlen(body) : 0;
    req.keep_alive = 0;
    char req_buf[1024];
    int req_len = http_build_request(&req, req_buf, sizeof(req_buf));
    if (req_len < 0 || req_len + req.content_length >= (int)sizeof(req_buf)) {
        tcp_close(sock);
        return -1;
    }
    if (body && req.content_length > 0) {
        memcpy(req_buf + req_len, body, req.content_length);
        req_len += req.content_length;
    }
    tcp_send(sock, (uint8_t *)req_buf, req_len);
    int total = 0;
    while (total < resp_size - 1) {
        int n = tcp_recv(sock, (uint8_t *)(resp_buf + total), resp_size - 1 - total);
        if (n <= 0) break;
        total += n;
        if (http_body_complete(resp_buf, total)) break;
    }
    resp_buf[total] = '\0';
    tcp_close(sock);
    return total;
}

int http_server_start(int port) {
    (void)port;
    return 0;
}

int http_server_stop(void) {
    return 0;
}

void http_server_set_handler(int (*handler)(const http_request_t *req, http_response_t *resp, char *body_buf, int body_size)) {
    g_http_handler = handler;
}
