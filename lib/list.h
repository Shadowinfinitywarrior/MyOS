#ifndef LIST_H
#define LIST_H

#include "../include/types.h"

/* Intrusive doubly-linked list (Linux-style) */
typedef struct list_node {
    struct list_node *next;
    struct list_node *prev;
} list_node_t;

#define LIST_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) list_node_t name = LIST_INIT(name)

#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - (size_t)(&((type *)0)->member)))

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, tmp, head) \
    for (pos = (head)->next, tmp = pos->next; \
         pos != (head); \
         pos = tmp, tmp = pos->next)

void list_init(list_node_t *head);
void list_add(list_node_t *node, list_node_t *head);
void list_add_tail(list_node_t *node, list_node_t *head);
void list_remove(list_node_t *node);
int  list_empty(list_node_t *head);
uint32_t list_count(list_node_t *head);

#endif

