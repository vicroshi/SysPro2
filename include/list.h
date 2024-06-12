//
// Created by vic on 6/9/24.
//

#ifndef SYSPRO2_LIST_H
#define SYSPRO2_LIST_H
#include <pthread.h>
typedef struct ListNode{
    pthread_t thread;
    struct ListNode* next;
} list_node;

typedef struct list{
    list_node* head;
    list_node* tail;
    int size;
} list;
void list_init(list*);
int list_add(list*, pthread_t);
int list_pop(list*, pthread_t*);
int list_remove(list*, pthread_t);
void list_destroy(list*);
#endif //SYSPRO2_LIST_H
