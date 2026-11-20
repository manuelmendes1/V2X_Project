#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Probe.h"
#include "oer_decoder.h"
#include "oer_encoder.h"

static int encode_value(uint8_t value, uint8_t *buffer,
                        size_t capacity, size_t *encoded_size)
{
    Probe_t source;
    asn_enc_rval_t result;

    memset(&source, 0, sizeof(source));

    source.eeType = calloc(1, sizeof(*source.eeType));
    if (!source.eeType) return -1;

    source.eeType->buf = malloc(1);
    if (!source.eeType->buf) {
        free(source.eeType);
        return -1;
    }

    source.eeType->buf[0] = value;
    source.eeType->size = 1;
    source.eeType->bits_unused = 0;

    result = oer_encode_to_buffer(
        &asn_DEF_Probe, NULL, &source, buffer, capacity
    );

    free(source.eeType->buf);
    free(source.eeType);

    if (result.encoded < 0) return -1;

    *encoded_size = (size_t)result.encoded;
    return 0;
}

static int decode_and_check(const uint8_t *buffer, size_t size,
                            uint8_t expected, const char *label)
{
    Probe_t *decoded = NULL;
    asn_dec_rval_t result;

    result = oer_decode(
        NULL, &asn_DEF_Probe, (void **)&decoded, buffer, size
    );

    if (result.code != RC_OK || result.consumed != size) {
        fprintf(stderr, "%s: DECODE_FAILED code=%d consumed=%zu\n",
                label, (int)result.code, result.consumed);
        ASN_STRUCT_FREE(asn_DEF_Probe, decoded);
        return -1;
    }

    if (!decoded || !decoded->eeType ||
        !decoded->eeType->buf ||
        decoded->eeType->size != 1 ||
        decoded->eeType->bits_unused != 0 ||
        decoded->eeType->buf[0] != expected) {
        fprintf(stderr, "%s: EE_TYPE_MISMATCH\n", label);
        ASN_STRUCT_FREE(asn_DEF_Probe, decoded);
        return -1;
    }

    printf("%s=PASS eeType=0x%02X size=%zu bits_unused=%d\n",
           label, decoded->eeType->buf[0],
           decoded->eeType->size, decoded->eeType->bits_unused);

    ASN_STRUCT_FREE(asn_DEF_Probe, decoded);
    return 0;
}

static void print_hex(const char *label, const uint8_t *buffer, size_t size)
{
    size_t i;
    printf("%s", label);
    for (i = 0; i < size; ++i) printf("%02X", buffer[i]);
    putchar('\n');
}

int main(void)
{
    uint8_t default_wire[64] = {0};
    uint8_t nondefault_wire[64] = {0};
    size_t default_size = 0, nondefault_size = 0;

    if (encode_value(0x80, default_wire, sizeof(default_wire),
                     &default_size) != 0) {
        puts("DEFAULT_ENCODING=FAIL");
        return 1;
    }

    if (encode_value(0x40, nondefault_wire, sizeof(nondefault_wire),
                     &nondefault_size) != 0) {
        puts("NONDEFAULT_ENCODING=FAIL");
        return 1;
    }

    printf("DEFAULT_WIRE_SIZE=%zu\n", default_size);
    printf("NONDEFAULT_WIRE_SIZE=%zu\n", nondefault_size);
    print_hex("DEFAULT_WIRE_HEX=", default_wire, default_size);
    print_hex("NONDEFAULT_WIRE_HEX=", nondefault_wire, nondefault_size);

    if (default_size >= nondefault_size) {
        puts("DEFAULT_OMISSION=FAIL");
        return 1;
    }
    puts("DEFAULT_OMISSION=PASS");

    if (decode_and_check(default_wire, default_size, 0x80,
                         "OMITTED_DEFAULT_RESTORED") != 0)
        return 1;

    if (decode_and_check(nondefault_wire, nondefault_size, 0x40,
                         "NONDEFAULT_PRESERVED") != 0)
        return 1;

    puts("FORK_OER_DEFAULT_ROUNDTRIP=PASS");
    return 0;
}
