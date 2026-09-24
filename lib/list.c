#include "list.h"
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"

void list_init(list_node_t *head) {
    head->next = head;
    head->prev = head;
}

void list_add(list_node_t *node, list_node_t *head) {
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
}

void list_add_tail(list_node_t *node, list_node_t *head) {
    node->next = head;
    node->prev = head->prev;
    head->prev->next = node;
    head->prev = node;
}

void list_remove(list_node_t *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

int list_empty(list_node_t *head) {
    return head->next == head;
}

uint32_t list_count(list_node_t *head) {
    uint32_t count = 0;
    list_node_t *pos;
    list_for_each(pos, head) count++;
    return count;
}

