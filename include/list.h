#ifndef ARRAY_LIST_H
#define ARRAY_LIST_H

#include <stdlib.h>

typedef struct list list_t;
typedef int (*compare_t)(void*, void*);

/**
 * Create a generic array list with the given initial capacity
 * @param initial_capacity the expected number of elements
 */
list_t* list_create(size_t initial_capacity);

/**
 * Destroy the contents of the array list with the given destructor, free
 * the array list and all allocated memory.
 * 
 * @param destroy an element destructor: unused if null
 */
void list_destroy(list_t *list, void (*destroy)(void *));

/**
 * Return the size of the list.
 * 
 * @return the size of the list
 */
size_t list_size(list_t *list);

/**
 * Add the element to the back of the list.
 * 
 * @return the ith element. 
 */
void list_add(list_t *list, void *e);

/**
 * Return the ith element in the list. Assert that the index is valid.
 * 
 * @return the ith element. 
 */
void* list_get(list_t *list, size_t i);

/**
 * Remove the ith element from the list and return the element.
 * Assert that the index is valid.
 * 
 * @return the ith element.
 */
void* list_remove(list_t *list, size_t i);

size_t list_find(list_t *list, void *e, compare_t cmp);

#endif /* ARRAY_LIST_H */