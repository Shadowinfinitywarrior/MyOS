#include "web.h"
#include "http.h"
#include "dns.h"
#include "../lib/string.h"
#include "../lib/printf.h"

int web_search(const char *query, char *result_buf, int result_size) {
    if (!query || !result_buf || result_size <= 0) return -1;
    // In a real implementation, this would query a search API
    // For now, construct a basic response
    snprintf(result_buf, result_size, "Search query: %s\nWeb search not fully implemented in this build - returning stub results.\n", query);
    return strlen(result_buf);
}

int web_fetch(const char *url, char *result_buf, int result_size) {
    if (!url || !result_buf || result_size <= 0) return -1;
    // Parse URL - very basic
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    } else if (strncmp(p, "https://", 8) == 0) {
        p += 8;
        // HTTPS not implemented, but we can try HTTP fallback if port 80
    }
    // Find host/path
    char host[128] = {0};
    char path[256] = {0};
    const char *slash = strchr(p, '/');
    if (slash) {
        int hlen = slash - p;
        if (hlen >= (int)sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        strcpy(path, slash);
    } else {
        strcpy(host, p);
        strcpy(path, "/");
    }
    // Remove port if present
    char *colon = strchr(host, ':');
    if (colon) *colon = '\0';
    uint32_t ip;
    if (dns_resolve(host, &ip) < 0) {
        snprintf(result_buf, result_size, "Failed to resolve host: %s", host);
        return -1;
    }
    int port = (strncmp(url, "https://", 8) == 0) ? HTTPS_PORT : HTTP_PORT;
    // For HTTPS, we'd need TLS - not implemented; try HTTP on same host if different
    if (port == HTTPS_PORT) {
        port = HTTP_PORT; // fallback for stub
    }
    return http_client_get(host, ip, port, path, result_buf, result_size);
}

int web_query(const char *query, char *result_buf, int result_size) {
    // Try to interpret as URL or search
    if (strstr(query, "http://") == query || strstr(query, "https://") == query) {
        return web_fetch(query, result_buf, result_size);
    }
    return web_search(query, result_buf, result_size);
}
