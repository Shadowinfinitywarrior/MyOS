#ifndef STRING_H
#define STRING_H

#include "../include/types.h"

size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
int strncasecmp(const char *s1, const char *s2, size_t n);
int tolower(int c);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strtok(char *s, const char *delim);
char *strstr(const char *haystack, const char *needle);
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
int snprintf(char *str, size_t size, const char *format, ...);

#endif
