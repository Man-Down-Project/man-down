#include <stdint.h>
#include <string.h>
#include "mbedtls/md.h"

#define KEY_LEN 16
#define AUTH_TAG_LEN 16

uint8_t shared_key[KEY_LEN] = {
    0x9A,0x4F,0x21,0xC7,
    0x55,0x13,0xE8,0x02,
    0x6D,0xB9,0x33,0xA1,
    0x7C,0x4D,0x90,0xEE
};

void generate_auth_tag(uint8_t *data, size_t data_len, uint8_t *auth_tag)
{
    uint8_t full_hash[32];   // SHA256 output

    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);

    mbedtls_md_setup(&ctx, md, 1);

    // Start HMAC
    mbedtls_md_hmac_starts(&ctx, shared_key, KEY_LEN);

    // Hash the packet data
    mbedtls_md_hmac_update(&ctx, data, data_len);

    // Finish HMAC
    mbedtls_md_hmac_finish(&ctx, full_hash);

    // Store truncated tag
    memcpy(auth_tag, full_hash, AUTH_TAG_LEN);

    mbedtls_md_free(&ctx);
}