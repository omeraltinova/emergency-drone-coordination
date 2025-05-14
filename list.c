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
    
    if (pthread_cond_init(&list->not_empty_cv, NULL) != 0) {
        perror("Not-empty CV initialization failed");
        pthread_mutex_destroy(&list->lock);
        free(list->startaddress);
        free(list);
        return NULL;
    }
    
    if (pthread_cond_init(&list->not_full_cv, NULL) != 0) {
        perror("Not-full CV initialization failed");
        pthread_cond_destroy(&list->not_empty_cv);
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

    pthread_mutex_lock(&list->lock);

    // Wait while the list is full
    while (list->number_of_elements >= list->capacity) {
        if (pthread_cond_wait(&list->not_full_cv, &list->lock) != 0) {
            perror("pthread_cond_wait failed on not_full_cv");
            pthread_mutex_unlock(&list->lock);
            return NULL;
        }
    }

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

        // Signal that the list is no longer empty
        pthread_cond_signal(&list->not_empty_cv);
    } else {
        // This case should ideally not be reached if capacity check is correct
        // and find_memcell_fornode is robust.
        // If it's reached, it implies an issue with find_memcell_fornode or logic.
        // No element was added, so no need to signal not_empty_cv.
        // We don't signal not_full_cv either because we didn't consume capacity due to error.
        perror("list is full or failed to find memory cell (unexpected in add after capacity check)");
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
        // Signal that the list is no longer full
        pthread_cond_signal(&list->not_full_cv);
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

    pthread_mutex_lock(&list->lock);

    // Wait while the list is empty
    while (list->number_of_elements == 0) {
        if (pthread_cond_wait(&list->not_empty_cv, &list->lock) != 0) {
            perror("pthread_cond_wait failed on not_empty_cv");
            pthread_mutex_unlock(&list->lock);
            return NULL;
        }
    }

    void *result_data = NULL;

    if (list->head != NULL) {
        // Copy data before removing node
        memcpy(dest, list->head->data, list->datasize);
        
        // Remove the node (removenode itself doesn't handle CVs or number_of_elements)
        if (removenode(list, list->head) == 0) { // removenode updates links and occupied status
            // list->number_of_elements is decremented inside removenode now
            result_data = dest;
            // Signal that the list is no longer full
            pthread_cond_signal(&list->not_full_cv);
        } else {
            // Should not happen if list->head was not NULL and list not empty
            perror("removenode failed unexpectedly in pop");
        }
    } else {
         // This case should not be reached if the while loop for not_empty_cv is correct
        perror("List became empty unexpectedly after wait in pop");
    }

    pthread_mutex_unlock(&list->lock);
    return result_data;
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
    // since it's only called from other synchronized functions (pop, removedata)
    
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
        list->number_of_elements--; // Decrement happens here now

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
    pthread_cond_destroy(&list->not_empty_cv);
    pthread_cond_destroy(&list->not_full_cv);

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