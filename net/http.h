#ifndef HTTP_H
#define HTTP_H

#include "../include/types.h"

#define HTTP_PORT 80
#define HTTPS_PORT 443

typedef struct {
    char method[16];
    char path[256];
    char version[16];
    char host[128];
    char user_agent[128];
    char content_type[64];
    int content_length;
    int keep_alive;
} http_request_t;

typedef struct {
    int status_code;
    char status_text[64];
    char content_type[64];
    int content_length;
    int keep_alive;
    char location[256];
} http_response_t;

int http_parse_request(const char *buf, int len, http_request_t *req);
int http_parse_response(const char *buf, int len, http_response_t *resp);
int http_build_request(http_request_t *req, char *buf, int buf_size);
int http_build_response(http_response_t *resp, const char *body, char *buf, int buf_size);

// HTTP client
int http_client_get(const char *host, uint32_t ip, int port, const char *path, char *resp_buf, int resp_size);
int http_client_post(const char *host, uint32_t ip, int port, const char *path, const char *body, const char *content_type, char *resp_buf, int resp_size);

// HTTP server
int http_server_start(int port);
int http_server_stop(void);
void http_server_set_handler(int (*handler)(const http_request_t *req, http_response_t *resp, char *body_buf, int body_size));

#endif
