/**
 * @file demo_protobuf_c.c
 * @brief protobuf-c runtime version + simple buffer append.
 */

#include "protobuf-c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("====================================================\n");
    printf("protobuf-c Runtime Demonstration\n");
    printf("====================================================\n");

    const char *ver = protobuf_c_version();
    const uint32_t ver_num = protobuf_c_version_number();
    printf("[1] Header version:  %s (%u)\n", PROTOBUF_C_VERSION, PROTOBUF_C_VERSION_NUMBER);
    printf("[2] Runtime version: %s (%u)\n", ver, ver_num);

    if (strcmp(ver, PROTOBUF_C_VERSION) != 0) {
        fprintf(stderr, "Header/runtime version mismatch\n");
        return 1;
    }

    uint8_t pad[64];
    ProtobufCBufferSimple simple = PROTOBUF_C_BUFFER_SIMPLE_INIT(pad);
    ProtobufCBuffer *buffer = (ProtobufCBuffer *)&simple;
    const uint8_t payload[] = { 0x0a, 0x06, 'z', 'a', 'p', 'o', 't', 'a' }; /* field 1, string "zapota" */
    buffer->append(buffer, sizeof(payload), payload);

    printf("[3] Packed sample field (proto3 string) %zu bytes:", simple.len);
    for (size_t i = 0; i < simple.len; i++) {
        printf(" %02x", simple.data[i]);
    }
    printf("\n");

    PROTOBUF_C_BUFFER_SIMPLE_CLEAR(&simple);
    printf("protobuf-c demonstration completed successfully.\n");
    return 0;
}
