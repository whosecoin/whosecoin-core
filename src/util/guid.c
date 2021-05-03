#include <sodium.h>
#include <stdio.h>

#include "util/guid.h"

guid_t guid_new() {
    guid_t result;
    result.i[0] = randombytes_random();
    result.i[1] = randombytes_random();
    result.i[2] = randombytes_random();
    result.i[3] = randombytes_random();
    return result;
}

int guid_compare(guid_t g1, guid_t g2) {
    for (int j = 0; j < 4; j++) {
        if (g1.i[j] < g2.i[j]) return -1;
        if (g1.i[j] > g2.i[j]) return +1;
    }
    return 0;
}

void guid_print(guid_t guid) {
    printf("%04x%04x%04x%04x\n", guid.i[0], guid.i[1], guid.i[2], guid.i[3]);
}

guid_t guid_null() {
    return (guid_t){0, 0, 0, 0};
}

int guid_is_null(guid_t guid) {
    return guid.i[0] == 0
        && guid.i[1] == 0
        && guid.i[2] == 0
        && guid.i[3] == 0;
}
