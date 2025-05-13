/**
 * @file list.c
 * @author adaskin
 * @brief  a simple doubly linked list stored in an array(contigous
 * memory). this program is written for educational purposes 
 * and may include some bugs.
 * TODO: add synchronization
 * @version 0.1
 * @date 2024-04-21
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "headers/list.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>


/**
 * @brief Create a list object, allocates new memory for list, and
 * sets its data members
 *
 * @param datasize: size of data in each node
 * @param capacity: maximum number of nodes can be stored in this list
 * @return List*
 */
List *create_list(size_t datasize, int capacity) {
    List *list = malloc(sizeof(List));
    memset(list, 0, sizeof(List));

    list->datasize = datasize;
    list->nodesize = sizeof(Node) + datasize;

    list->startaddress = malloc(list->nodesize * capacity);
    list->endaddress =
        list->startaddress + (list->nodesize * capacity);
    memset(list->startaddress, 0, list->nodesize * capacity);

    list->lastprocessed = (Node *)list->startaddress;

    list->number_of_elements = 0;
    list->capacity = capacity;

    // Initialize synchronization primitives
    if (pthread_mutex_init(&list->lock, NULL) != 0) {
        perror("Mutex initialization failed");
        free(list->startaddress);
        free(list);
        return NULL;
    }
    
    if (sem_init(&list->elements_sem, 0, 0) != 0) {
        perror("Elements semaphore initialization failed");
        pthread_mutex_destroy(&list->lock);
        free(list->startaddress);
        free(list);
        return NULL;
    }
    
    if (sem_init(&list->capacity_sem, 0, capacity) != 0) {
        perror("Capacity semaphore initialization failed");
        sem_destroy(&list->elements_sem);
        pthread_mutex_destroy(&list->lock);
        free(list->startaddress);
        free(list);
        return NULL;
    }

    /*ops*/
    list->self = list;
    list->add = add;
    list->removedata = removedata;
    list->removenode = removenode;
    list->pop = pop;
    list->peek = peek;
    list->destroy = destroy;
    list->printlist = printlist;
    list->printlistfromtail = printlistfromtail;
    return list;
}
/**
 * @brief finds a memory cell in the mem area of list
 * @param list
 * @return Node*
 */
static Node *find_memcell_fornode(List *list) {
    Node *node = NULL;
    /*search lastprocessed---end*/
    Node *temp = list->lastprocessed;
    while ((char *)temp < list->endaddress) {
        if (temp->occupied == 0) {
            node = temp;
            break;
        } else {
            temp = (Node *)((char *)temp + list->nodesize);
        }
    }
    if (node == NULL) {
        /*search startaddress--lastprocessed*/
        temp = (Node *)list->startaddress;
        while (temp < list->lastprocessed) {
            if (temp->occupied == 0) {
                node = temp;
                break;
            } else {
                temp = (Node *)((char *)temp + list->nodesize);
            }
        }
    }
    return node;
}

/**
 * @brief find an unoccupied node in the array, and makes a node with
 * the given data and ADDS it to the HEAD of the list
 * @param list:
 * @param data: a data addrress, its size is determined from
 * list->datasize
 * @return * find,*
 */
Node *add(List *list, void *data) {
    Node *node = NULL;

    // Wait for available capacity
    if (sem_wait(&list->capacity_sem) != 0) {
        perror("sem_wait failed on capacity_sem");
        return NULL;
    }

    // Lock the list for modification
    pthread_mutex_lock(&list->lock);

    /*first find an unoccupied memcell and insert into it*/
    node = find_memcell_fornode(list);

    if (node != NULL) {
        /*create_node*/
        node->occupied = 1;
        memcpy(node->data, data, list->datasize);

        /*change new node into head*/
        if (list->head != NULL) {
            Node *oldhead = list->head;
            oldhead->prev = node;
            node->prev = NULL;
            node->next = oldhead;
        }

        list->head = node;
        list->lastprocessed = node;
        list->number_of_elements += 1;
        if (list->tail == NULL) {
            list->tail = list->head;
        }

        // Signal that a new element is available
        sem_post(&list->elements_sem);
    } else {
        // If we couldn't find a node, return the capacity semaphore
        sem_post(&list->capacity_sem);
        perror("list is full!");
    }

    pthread_mutex_unlock(&list->lock);
    return node;
}
/**
 * @brief finds the node with the value same as the mem pointed by
 * data and removes that node. it returns temp->node
 * @param list
 * @param data
 * @return int: in success, it returns 0; if not found it returns 1.
 */
int removedata(List *list, void *data) {
    int result = 1;  // Default to "not found"
    
    pthread_mutex_lock(&list->lock);
    
    Node *temp = list->head;
    while (temp != NULL &&
           memcmp(temp->data, data, list->datasize) != 0) {
        temp = temp->next;
    }
    if (temp != NULL) {
        Node *prevnode = temp->prev;
        Node *nextnode = temp->next;
        
        // Update head if removing first node
        if (temp == list->head) {
            list->head = nextnode;
        }
        
        // Update tail if removing last node
        if (temp == list->tail) {
            list->tail = prevnode;
        }
        
        if (prevnode != NULL) {
            prevnode->next = nextnode;
        }
        if (nextnode != NULL) {
            nextnode->prev = prevnode;
        }

        temp->next = NULL;
        temp->prev = NULL;
        temp->occupied = 0;
        
        list->number_of_elements--;
        sem_post(&list->capacity_sem);  // Return capacity
        result = 0;  // Success
    }
    
    pthread_mutex_unlock(&list->lock);
    return result;
}
/**
 * @brief removes the node from list->head, and copies its data into
 * dest, also returns it.
 * @param list
 * @param dest: address to cpy data
 * @return void*: if there is data, it returns address of dest; else
 * it returns NULL.
 */
void *pop(List *list, void *dest) {
    if (!list || !dest) {
        return NULL;
    }

    // Wait for an element to be available
    if (sem_wait(&list->elements_sem) != 0) {
        perror("sem_wait failed on elements_sem");
        return NULL;
    }

    pthread_mutex_lock(&list->lock);
    void *result = NULL;

    if (list->head != NULL) {
        // Copy data before removing node
        memcpy(dest, list->head->data, list->datasize);
        
        // Remove the node
        if (removenode(list, list->head) == 0) {
            result = dest;
            // Signal that capacity is available
            sem_post(&list->capacity_sem);
        } else {
            // If removal failed, signal back to elements semaphore
            sem_post(&list->elements_sem);
        }
    } else {
        // No elements available, signal back to elements semaphore
        sem_post(&list->elements_sem);
    }

    pthread_mutex_unlock(&list->lock);
    return result;
}
/**
 * @brief returns the data stored in the head of the list
 * @param list
 * @return void*: returns the address of head->data
 */
void *peek(List *list) {
    void *result = NULL;
    
    pthread_mutex_lock(&list->lock);
    if (list->head != NULL) {
        result = list->head->data;
    }
    pthread_mutex_unlock(&list->lock);
    
    return result;
}

/**
 * @brief removes the given node from the list, it returns removed
 * node.
 * @param list
 * @param node
 * @return int: in sucess, it returns 0; if node not found, it
 * returns 1.
 */
int removenode(List *list, Node *node) {
    // Note: This function assumes the list mutex is ALREADY locked by the caller
    // since it's only called from other synchronized functions
    
    if (node != NULL) {
        Node *prevnode = node->prev;
        Node *nextnode = node->next;
        
        // Update links
        if (prevnode != NULL) {
            prevnode->next = nextnode;
        }
        if (nextnode != NULL) {
            nextnode->prev = prevnode;
        }
        
        // Clear node links
        node->next = NULL;
        node->prev = NULL;
        node->occupied = 0;

        // Update list metadata
        list->number_of_elements--;

        // Update head/tail pointers
        if (node == list->tail) {
            list->tail = prevnode;
        }
        if (node == list->head) {
            list->head = nextnode;
        }
        
        // Update last processed pointer
        // list->lastprocessed = node; // Commented out: Not safe to point to a removed node for find_memcell logic.
        
        return 0;
    }
    return 1;
}

/**
 * @brief deletes everything
 *
 * @param list
 */
void destroy(List *list) {
    // Clean up synchronization primitives
    pthread_mutex_destroy(&list->lock);
    sem_destroy(&list->elements_sem);
    sem_destroy(&list->capacity_sem);

    // Free allocated memory
    free(list->startaddress);
    memset(list, 0, sizeof(List));
    free(list);
}

/**
 * @brief prints list starting from head
 *
 * @param list
 * @param print: aprint function for the object data.
 */
void printlist(List *list, void (*print)(void *)) {
    if (!list) return;
    pthread_mutex_lock(&list->lock);
    Node *temp = list->head;
    printf("List (Head to Tail):\n");
    while (temp != NULL) {
        print(temp->data);
        temp = temp->next;
    }
    printf("End of List\n");
    pthread_mutex_unlock(&list->lock);
}
/**
 * @brief print list starting from tail
 *
 * @param list
 * @param print: print function
 */
void printlistfromtail(List *list, void (*print)(void *)) {
    if (!list) return;
    pthread_mutex_lock(&list->lock);
    Node *temp = list->tail;
    printf("List (Tail to Head):\n");
    while (temp != NULL) {
        print(temp->data);
        temp = temp->prev;
    }
    printf("End of List (from Tail)\n");
    pthread_mutex_unlock(&list->lock);
}