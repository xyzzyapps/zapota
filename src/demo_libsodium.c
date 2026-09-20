/**
 * @file demo_libsodium.c
 * @brief libsodium: init + BLAKE2b generichash.
 */

#include <sodium.h>
#include <stdio.h>

int main(void) {
    printf("====================================================\n");
    printf("libsodium Demonstration\n");
    printf("====================================================\n");

    if (sodium_init() < 0) {
        fprintf(stderr, "sodium_init failed\n");
        return 1;
    }
    printf("[1] %s\n", sodium_version_string());

    unsigned char hash[crypto_generichash_BYTES];
    const unsigned char msg[] = "zapota";
    if (crypto_generichash(hash, sizeof hash, msg, sizeof msg - 1, NULL, 0) != 0) {
        fprintf(stderr, "crypto_generichash failed\n");
        return 1;
    }

    printf("[2] BLAKE2b(\"zapota\") = ");
    for (size_t i = 0; i < sizeof hash; i++) printf("%02x", hash[i]);
    printf("\n");
    printf("libsodium demonstration completed successfully.\n");
    return 0;
}
