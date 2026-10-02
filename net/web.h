#ifndef WEB_H
#define WEB_H

#include "../include/types.h"

int web_fetch(const char *url, char *result_buf, int result_size);
int web_search(const char *query, char *result_buf, int result_size);
int web_query(const char *query, char *result_buf, int result_size);

#endif
