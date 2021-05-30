#include "test_util.h"
#include <assert.h>
#include <util/heap.h>

static int long_cmp(void *a, void *b) {
    return (long) a - (long) b;
}

void test_create() {
    heap_t *heap = heap_create(long_cmp, NULL);
    heap_destroy(heap);
}

void test_empty() {
    heap_t *heap = heap_create(long_cmp, NULL);
    assert(heap_size(heap) == 0);
    heap_destroy(heap);
}

void test_add() {
    heap_t *heap = heap_create(long_cmp, NULL);
    
    heap_add(heap, (void *) 3);
    assert(heap_size(heap) == 1);
    assert(heap_top(heap) == (void *) 3);

    heap_add(heap, (void *) 1);
    assert(heap_size(heap) == 2);
    assert(heap_top(heap) == (void *) 1);
    
    heap_add(heap, (void *) 2);
    assert(heap_size(heap) == 3);
    assert(heap_top(heap) == (void *) 1);

    assert(heap_pop(heap) == (void *) 1);
    assert(heap_pop(heap) == (void *) 2);
    assert(heap_pop(heap) == (void *) 3);

    assert(heap_size(heap) == 0);

    heap_destroy(heap);
}


int main(int argc, char *argv[]) {
    DO_TEST(test_create)
    DO_TEST(test_empty)
    DO_TEST(test_add)
}