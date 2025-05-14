/**
 * @file list.h
 * @author adaskin
 * @brief 
 * @version 0.1
 * @date 2024-04-21
 * 
 * @copyright Copyright (c) 2024
 */
#ifndef LIST_H
#define LIST_H

#include <time.h>
#include <pthread.h>


typedef struct node {
    struct node *prev;
    struct node *next;
    // size_t size; /*sizes are fixed for convenience*/
    char occupied;
    char data[];
} Node;

typedef struct list {
    Node *head;
    Node *tail;
    int number_of_elements;
    int capacity;
    int datasize;
    int nodesize;
    char *startaddress;
    char *endaddress;
    Node *lastprocessed;
    Node *free_list;

    pthread_mutex_t lock;        // Mutex for general list operations
    pthread_cond_t not_empty_cv; // CV for when list is not empty (replaces elements_sem)
    pthread_cond_t not_full_cv;  // CV for when list is not full (replaces capacity_sem)
    
    /*ops on the list*/
    Node *(*add)(struct list *list, void *data);
    int  (*removedata)(struct list *list, void *data);
    int (*removenode)(struct list *list, Node *node); 
    void *(*pop)(struct list *list, void* dest);
    void *(*peek)(struct list *list);
    void (*destroy)(struct list *list);
    void (*printlist)(struct list *list, void (*print)(void*));
    void (*printlistfromtail)(struct list *list, void (*print)(void*));

    struct list *self;     
} List;

List *create_list(size_t datasize, int capacity);
int removenode(List *list, Node *node);
Node *add(List *list, void *data);
int removedata(List *list, void *data);
void *pop(List *list, void *dest);
void *peek(List *list);
void destroy(List *list);
void printlist(List *list, void (*print)(void*));
void printlistfromtail(List *list, void (*print)(void*));

#endif
// LIST_H