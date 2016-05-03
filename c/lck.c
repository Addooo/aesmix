#include <openssl/evp.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "lck.h"

#ifdef DEBUG
#define DEBUG_TEST 1
#else
#define DEBUG_TEST 0
#endif

#define debug_print(...) \
    do { if (DEBUG_TEST) fprintf(stderr, __VA_ARGS__); } while (0)

static inline void do_step(
    unsigned char* macro,
    unsigned int step,
    unsigned char* key
){
    unsigned int i, off, mask, start, dist;
    unsigned char temp[BLOCK_SIZE];
    int outlen1;
    EVP_CIPHER_CTX ctx;

    EVP_EncryptInit(&ctx, EVP_aes_128_ecb(), key, NULL);
    EVP_CIPHER_CTX_set_padding(&ctx, 0); // disable padding

    mask = ((1 << DOF) - 1) << (step * DOF);
    dist = 1 << (step * DOF);
    debug_print("STEP: %d (distance: %d)\n", step, dist);

    for (start=0; start < (1 << DIGITS); start=((start|mask)+1) & ~mask) {
        debug_print("GROUP: ");
        for (i=0, off=start; i < MINI_PER_BLOCK; ++i, off+=dist) {
            debug_print("%d ", off);
            memcpy(&temp[i*MINI_SIZE], &macro[off*MINI_SIZE], MINI_SIZE);
        }

        EVP_EncryptUpdate(&ctx, temp, &outlen1, temp, BLOCK_SIZE);
        assert(outlen1 == BLOCK_SIZE);

        for (i=0, off=start; i < MINI_PER_BLOCK; ++i, off+=dist) {
            memcpy(&temp[i*MINI_SIZE], &macro[off*MINI_SIZE], MINI_SIZE);
        }

        debug_print("\n");
    }
}

void encrypt_macroblock(
    unsigned char* macro,
    unsigned char* out,
    unsigned char* key,
    unsigned char* iv
){
    int outlen1, outlen2;
    unsigned int step;
    EVP_CIPHER_CTX ctx;

    // Step 0 is always a CTR encryption
    EVP_EncryptInit(&ctx, EVP_aes_128_ctr(), key, iv);
    EVP_EncryptUpdate(&ctx, out, &outlen1, macro, MACRO_SIZE);
    EVP_EncryptFinal(&ctx, &out[outlen1], &outlen2);
    assert(outlen1 + outlen2 == MACRO_SIZE);

    // Step 1-N are always ECB encryptions
    for (step=1; step < DIGITS/DOF; ++step) {
        do_step(out, step, key);
    }
}

void decrypt_macroblock(
    unsigned char* macro,
    unsigned char* out,
    unsigned char* key,
    unsigned char* iv
){
    int outlen1, outlen2;
    unsigned int step;
    EVP_CIPHER_CTX ctx;

    // Step 1-N are always ECB encryptions
    memcpy(out, macro, MACRO_SIZE);
    for (step = DIGITS/DOF - 1; step >= 1; --step) {
        do_step(out, step, key);
    }

    // Step 0 is always a CTR encryption
    EVP_DecryptInit(&ctx, EVP_aes_128_ctr(), key, iv);
    EVP_DecryptUpdate(&ctx, out, &outlen1, out, MACRO_SIZE);
    EVP_DecryptFinal(&ctx, &out[outlen1], &outlen2);
    assert(outlen1 + outlen2 == MACRO_SIZE);
}

void encrypt(
    unsigned char* data,
    unsigned char* out,
    unsigned long size,
    unsigned char* key,
    unsigned char* iv
){
    unsigned long offset;
    assert(size % MACRO_SIZE == 0);
    for (offset=0; offset < size; offset+=MACRO_SIZE) {
        debug_print("ENCRYPT BLOCK with offset: %lu\n", offset);
        encrypt_macroblock(&data[offset], &out[offset], key, iv);
    }
}

void decrypt(
    unsigned char* data,
    unsigned char* out,
    unsigned long size,
    unsigned char* key,
    unsigned char* iv
){
    unsigned long offset;
    assert(size % MACRO_SIZE == 0);
    for (offset=0; offset < size; offset+=MACRO_SIZE) {
        debug_print("DECRYPT BLOCK with offset: %lu\n", offset);
        decrypt_macroblock(&data[offset], &out[offset], key, iv);
    }
}
