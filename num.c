#include "mcp_forth.h"

int num_encoded_size_from_int(int num) {
    uint8_t buf[5];
    num_encode(num, buf);
    return num_encoded_size_from_encoded(buf);
}

void num_encode(int num, uint8_t * dst) {
    union {int i; unsigned u;} iu;
    iu.i = num;
    unsigned u = iu.u;

    bool negative = num < 0;

    if(negative) u = ~u;

    while(u & ~0x3fu) { /* while there are bits set above the 6th */
        *(dst++) = (u & 0x7fu) | 0x80u; /* mask lower 7 bits and set the 8th bit */
        u >>= 7u;
    }

    if(negative) u |= 0x40u; /* set the sign bit (7th bit) */

    *dst = u; /* the 8th bit is clear */
}

int num_encoded_size_from_encoded(const uint8_t * src) {
    const uint8_t * p = src;
    while(*(p++) & 0x80);
    return p - src;
}

int num_decode(const uint8_t * src) {
    int len = num_encoded_size_from_encoded(src);
    unsigned b = src[len - 1];
    bool negative = b & 0x40u;
    unsigned u = b & 0x3fu;
    for(int i=len-2; i >= 0; i--) {
        b = src[i];
        u <<= 7u;
        u |= b & 0x7fu;
    }

    if(negative) u = ~u;

    union {int i; unsigned u;} iu;
    iu.u = u;
    return iu.i;
}
