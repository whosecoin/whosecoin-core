#include <util/heap.h>

#include <assert.h>
#include <stdlib.h>

#define HEAP_DEFAULT_CAPACITY 4

typedef struct heap {
    void **data;
    size_t capacity;
    size_t size;
    comparator_t e_compare;
    destructor_t e_destroy;
} heap_t;

/**
 * Return the index of the parent of the ith element in binary tree.
 * @param i the element index
 * @return the parent index
 */
static size_t parent(size_t i) {
    return (i - 1) / 2;
}

/**
 * Return the index of the left child of the ith element in binary tree.
 * @param i the element index
 * @return the child index 
 */
static size_t left_child(size_t i) {
    return (2 * i) + 1;
}

/**
 * Return the index of the right child of the ith element in binary tree
  @param i the element index
 * @return the child index
 */
static size_t right_child(size_t i) {
    return (2 * i) + 2;
}

/**
 * Swap the elements with indices a and b.
 * @param self the heap
 * @param a the index of the first element
 * @param b the index of the second element
 */
static void swap_element(heap_t *self, size_t a, size_t b) {
    void *tmp = self->data[a];
    self->data[a] = self->data[b];
    self->data[b] = tmp;
}

/**
 * Compare the elements with indices a and b. Return a negative number
 * if a < b. Return a positive number if a > b. Return zero if a = b.
 * 
 * @param self the heap
 * @param a the index of the first element
 * @param b the index of the second element
 */
static int compare(heap_t *self, size_t a, size_t b) {
    void *e1 = heap_get(self, a);
    void *e2 = heap_get(self, b);
    return self->e_compare(e1, e2);
}

/**
 * Repeatedly swap the ith element with its parent until the element
 * is moved to the top of the heap.
 * 
 * @param self the heap
 * @param i the element index
 */
static void move_to_top(heap_t *self, size_t i) {
    while (i > 0) {
        swap_element(self, parent(i), i);
        i = parent(i);
    }
}

/**
 * Repeatedly swap the ith element with its parent while the element
 * has a priority less than its parent.
 * 
 * @param self the heap
 * @param i the element index
 */
static void shift_up(heap_t *self, size_t i) {
    while (i > 0 && compare(self, i, parent(i)) < 0) {
        swap_element(self, parent(i), i);
        i = parent(i);
    }
}

/**
 * Repeatadly swap the ith element with the child of smallest priority
 * until the element has a smaller priority than either of its child.
 * 
 * @param self the heap
 * @param i the element index
 */
static void shift_down(heap_t *self, size_t i) {
    size_t max_i = i;

    size_t l = left_child(i);
    if (l < self->size && compare(self, l, max_i) < 0) {
        max_i = l;
    }

    size_t r = right_child(i);
    if (r < self->size && compare(self, r, max_i) < 0) {
        max_i = r;
    }
    
    if (i != max_i) {
        swap_element(self, i, max_i);
        shift_down(self, max_i);
    }
}


heap_t* heap_create(comparator_t e_compare, destructor_t e_destroy) {
    heap_t *res = calloc(1, sizeof(heap_t));
    assert(res != NULL);
    res->size = 0;
    res->capacity = HEAP_DEFAULT_CAPACITY;
    res->data = malloc(res->capacity * sizeof(void*));
    res->e_compare = e_compare;
    res->e_destroy = e_destroy;
    return res;
}

void heap_destroy(heap_t *self) {
    if (self == NULL) return;
    if (self->e_destroy) {
        for (size_t i = 0; i < self->size; i++) {
            self->e_destroy(self->data[i]);
        }
    }
    free(self->data);
    free(self);
}

size_t heap_size(heap_t *self) {
    return self->size;
}

void* heap_get(heap_t *self, size_t i) {
    assert(0 <= i && i < self->size);
    return self->data[i];
}

void heap_add(heap_t *self, void *e) {

    /* resize the buffer if it is already full */
    if (self->size == self->capacity) {
        self->capacity *= 2;
        self->data = realloc(self->data, self->capacity * sizeof(void*));
        assert(self->data != NULL);
    }

    /* add the element to the bottom row of the tree */
    self->data[self->size] = e;
    self->size += 1;

    /* swap elements until the heap property is satisfied */
    shift_up(self, self->size - 1);
}

void* heap_remove(heap_t *self, size_t i) {
    assert(0 <= i && i < self->size);
    move_to_top(self, i);
    return heap_pop(self);
}

void* heap_top(heap_t *self) {
    if (heap_size(self) == 0) return NULL;
    return heap_get(self, 0);
}

void* heap_pop(heap_t *self) {
    if (heap_size(self) == 0) return NULL;
    void *res = self->data[0];
    
    /* move the last element to the top of the heap */
    self->size -= 1;
    self->data[0] = self->data[self->size];
    
    /* swap elements until the heap property is satisfied */
    shift_down(self, 0);

    return res;
}
