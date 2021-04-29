#include "test_util.h"
#include "array_list.h"
#include <assert.h>

void test_empty() {
    array_list_t *list = array_list_create(4);
    assert(array_list_size(list) == 0);
    assert_error(array_list_remove(list, 0));
    assert_error(array_list_get(list, 0));
    array_list_destroy(list, NULL);
}

void test_add() {
    int foo;
    array_list_t *list = array_list_create(0);
    assert(array_list_size(list) == 0);
    assert_error(array_list_get(list, 0));
    array_list_add(list, &foo);
    assert(array_list_get(list, 0) == &foo);
    assert(array_list_size(list) == 1);
    array_list_destroy(list, NULL);
}

void test_remove() {
    array_list_t *list = array_list_create(0);
    array_list_add(list, (void *) 1);
    array_list_add(list, (void *) 2);
    array_list_add(list, (void *) 3);
    array_list_add(list, (void *) 4);

    assert(array_list_size(list) == 4);
    assert(array_list_get(list, 0) == (void *) 1);
    assert(array_list_get(list, 1) == (void *) 2);
    assert(array_list_get(list, 2) == (void *) 3);
    assert(array_list_get(list, 3) == (void *) 4);

    array_list_remove(list, 1);
    assert(array_list_size(list) == 3);
    assert(array_list_get(list, 0) == (void *) 1);
    assert(array_list_get(list, 1) == (void *) 3);
    assert(array_list_get(list, 2) == (void *) 4);

    array_list_remove(list, 0);
    assert(array_list_size(list) == 2);
    assert(array_list_get(list, 0) == (void *) 3);
    assert(array_list_get(list, 1) == (void *) 4);

    array_list_remove(list, 1);
    assert(array_list_size(list) == 1);
    assert(array_list_get(list, 0) == (void *) 3);

    array_list_remove(list, 0);
    assert(array_list_size(list) == 0);
}

int main(int argc, char *argv[]) {
    DO_TEST(test_empty)
    DO_TEST(test_add)
    DO_TEST(test_remove)
}