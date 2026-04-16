#pragma once


#define PROV_KEY_LEN 16
#define PROV_MAX_CERT 2048

typedef struct __attribute__((packed)) {
    uint32_t version;
    uint32_t timestamp;
    uint8_t key[PROV_KEY_LEN];
    uint16_t cert_len;
    uint8_t cert[];
} provision_packet_t;

void handle_provision(uint8_t *data, int len);
void secure_memzero(void *v, size_t n);