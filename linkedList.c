/** 
* @file linkedList.c
* @brief Singly Linked List ADT
* @author Dave Reisner
* @version 1.00
* @date 2010-03-12
*/

#include <stdlib.h>
#include <stdio.h>

#include "linkedList.h"

/**
* @brief Add an element to a list
*
* @param l      List to add to
* @param data   Element to be added
*/
void llist_add(llist** l, void* data) {

    /* Fail if we were passed garbage */
    if (l == NULL) return;

    /* Create the new node; fail if out of memory */
    llist* n = malloc(sizeof(llist));
    if (n == NULL) {
        fprintf(stderr, "malloc() failed!\n");
        return;
    }

    /* Set the value and links for the new node */
    n->next = *l;
    *l = n;
    n->data = data;

    /* Update list pointer */

}

/**
* @brief Remove a single element from a list
*
* @param l      Element to be removed
*/
void llist_remove_node(llist** l) {
    if (l != NULL && *l != NULL) {
        llist* n = *l;
        *l = (*l)->next;
        free(n);
    }
}

/**
* @brief Delete an entire list
*
* @param l      List to be deleted
*/
void llist_delete(llist** l) {
    if (l == NULL || *l == NULL) {
        return;
    }

    /* Iterate through list and free each node */
    while ((*l)->next != NULL) {
        llist_remove_node(l);
        //*l = (*l)->next;
    }

    /* Free head of list */
    free(*l);
}

/**
* @brief Search for an element in a list
*
* @param ll     List to search
* @param data   Element to be searched for
* @param fn     Comparator function
*
* @return       Address of element or NULL
*/
llist** llist_search(llist** ll, const void* data, llist_fn_cmp fn) {
    if (ll == NULL) return NULL;

    while (*ll != NULL) {
        if ((*ll)->data && fn((*ll)->data, data) == 0) {
            return ll;
        }
        ll = &(*ll)->next;
    }
    return NULL;
}

