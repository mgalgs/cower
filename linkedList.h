/*
 *  linkedList.h
 */

#ifndef _LLIST_H
#define _LLIST_H

/**
* @brief Singly linked list ADT
*/
typedef struct __llist {
    /** Data held by the node */
    void *data;
    /** Pointer to the next node */
    struct __llist *next;
} llist;

typedef int (*llist_fn_cmp)(const void*, const void*); /* item comparison callback */

/* Accessors */
void llist_add(llist**, void*);
llist** llist_search(llist**, const void*, llist_fn_cmp);

/* Mutators */
void llist_remove_node(llist**);
void llist_delete(llist**);

#endif /* _LLIST_H */
