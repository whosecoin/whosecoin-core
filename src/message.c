#include <uv.h>

#include <message.h>

guid_t message_get_guid(uint8_t *message) {
    guid_t result;
    for (int j = 0; j < 4; j++) {
        result.i[j] = ntohl(((uint32_t *) message)[2 + j]);
    }
    return result;
}

void message_set_guid(uint8_t *message, guid_t guid) {
    for (int j = 0; j < 4; j++) {
        ((uint32_t *) message)[2 + j] = htonl(guid.i[j]);
    }
}

uint32_t message_get_length(uint8_t *message) {
    return ntohl(((uint32_t *) message)[1]);
}

void message_set_length(uint8_t *message, uint32_t length) {
    ((uint32_t *) message)[1] = htonl(length);
}

uint32_t message_get_magic(uint8_t *message) {
    return ntohl(((uint32_t *) message)[0]);
}

void message_set_magic(uint8_t *message, uint32_t magic) {
    ((uint32_t *) message)[0] = htonl(magic);
}

uint16_t message_get_type(uint8_t *message) {
    return ntohs(*((uint16_t *)(message + 24)));
}

void message_set_type(uint8_t *message, uint16_t type) {
    *((uint16_t *)(message + 24)) = htons(type);
}
