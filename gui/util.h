#ifndef GUI_UTIL_H
#define GUI_UTIL_H

#include "text.h"
#include "blit.h"

/* Small formatting helpers for on-screen text.
 *
 * The kernel's ksprintf supports only fixed-width zero padding, which is not
 * enough for tables like the process monitor (left-aligned padded columns).
 * These helpers cover the shapes the GUI actually needs, and they never
 * allocate. */

static inline void ui_utoa(uint64_t v, char *out, int cap) {
    char tmp[24];
    int n = 0;
    if (v == 0) tmp[n++] = '0';
    while (v > 0 && n < (int)sizeof(tmp)) { tmp[n++] = (char)('0' + (v % 10)); v /= 10; }
    int k = 0;
    while (n > 0 && k < cap - 1) out[k++] = tmp[--n];
    out[k] = 0;
}

/* Append a C string to `out` (capacity `cap`), never overflowing. */
static inline void ui_cat(char *out, int cap, const char *s) {
    int n = 0;
    while (out[n] && n < cap - 1) n++;
    for (int i = 0; s[i] && n < cap - 1; i++) out[n++] = s[i];
    out[n] = 0;
}

static inline void ui_strcpy(char *out, int cap, const char *s) {
    int n = 0;
    while (s[n] && n < cap - 1) { out[n] = s[n]; n++; }
    out[n] = 0;
}

/* Append a decimal number, optionally followed by a unit suffix. */
static inline void ui_cat_num(char *out, int cap, uint64_t v, const char *suffix) {
    int n = 0;
    while (out[n] && n < cap - 1) n++;
    char tmp[24];
    int k = 0;
    if (v == 0) tmp[k++] = '0';
    while (v > 0 && k < (int)sizeof(tmp)) { tmp[k++] = (char)('0' + (v % 10)); v /= 10; }
    while (k > 0 && n < cap - 1) out[n++] = tmp[--k];
    for (int i = 0; suffix[i] && n < cap - 1; i++) out[n++] = suffix[i];
    out[n] = 0;
}

/* Build "<n> B" into `out`. */
static inline void ui_bytes(uint64_t bytes, char *out, int cap) {
    ui_strcpy(out, cap, "");
    ui_cat_num(out, cap, bytes, " B");
}

/* Left-aligned in a field of `w` columns (truncates if too long). */
static inline void ui_pad_right(char *s, int w) {
    int n = 0;
    while (s[n]) n++;
    while (n < w && n < 63) s[n++] = ' ';
    s[n] = 0;
}

/* Right-aligned in a field of `w` columns. */
static inline void ui_pad_left(char *s, int w) {
    int n = 0;
    while (s[n]) n++;
    int pad = w - n;
    if (pad <= 0) return;
    char tmp[64];
    for (int i = 0; i < pad && i < 63; i++) tmp[i] = ' ';
    for (int i = 0; s[i] && i < 63 - pad; i++) tmp[pad + i] = s[i];
    tmp[pad + n] = 0;
    for (int i = 0; tmp[i]; i++) s[i] = tmp[i];
    s[pad + n] = 0;
}

#endif
