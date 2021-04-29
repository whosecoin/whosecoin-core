#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define DO_TEST(test_fn) \
    test_fn(); \
    printf("[PASS] %s\n", #test_fn);

#define assert_error(expr) \
    if (fork()) { \
        int status; \
        wait(&status); \
        int aborted = WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT; \
        assert(aborted); \
    } else { \
        freopen("/dev/null", "w", stderr); \
        expr; \
        exit(0); \
    }

#endif /* TEST_UTIL_H */
