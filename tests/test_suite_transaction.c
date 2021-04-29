#include "test_util.h"
#include <assert.h>

void test_true() {
    assert(1);
}

int main(int argc, char *argv[]) {
    DO_TEST(test_true)
}