//
// Created by vic on 6/9/24.
//

#include "list.h"
#include <stdlib.h>
void list_init(list* list) {
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

int list_add(list* list, pthread_t thread) {
    list_node* node = malloc(sizeof(list_node));
    if (node == NULL) {
        return 0;
    }
    node->thread = thread;
    node->next = NULL;
    if (list->head == NULL) {
        list->head = node;
    } else {
        list->tail->next = node;
    }
    list->tail = node;
    list->size++;
    return list->size-1;
}

int list_pop(list* list, pthread_t* thread) {
    if (list->head == NULL) {
        return 0;
    }
    list_node* node = list->head;
    list->head = node->next;
    if (list->head == NULL) {
        list->tail = NULL;
    }
    *thread = node->thread;
    free(node);
    list->size--;
    return 1;
}

int list_remove(list* list, pthread_t thread) {
    list_node* node = list->head;
    list_node* prev = NULL;
    while (node != NULL) {
        if (pthread_equal(node->thread, thread)) {
            if (prev == NULL) {
                list->head = node->next;
            } else {
                prev->next = node->next;
            }
            if (node == list->tail) {
                list->tail = prev;
            }
            free(node);
            list->size--;
            return 1;
        }
        prev = node;
        node = node->next;
    }
    return 0;
}

void list_destroy(list* list) {
    list_node* node = list->head;
    while (node != NULL) {
        list_node* next = node->next;
        free(node);
        node = next;
    }
}