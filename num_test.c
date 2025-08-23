#include "mcp_forth.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned iu(int x)
{
    union {int i; unsigned u;} iu;
    iu.i = x;
    return iu.u;
}

static void do_test(const void * v) {
    const int * v2 = v;
    int x = *v2;
    uint8_t buf[5];
    m4_num_encode(x, buf);
    int y = m4_num_decode(buf, NULL);
    if(x != y) {
        printf("fail. i=%d,%d u=%u,%u\n", x, y, iu(x), iu(y));
        exit(1);
    }
}

int main() {
    puts("unsigned");
    unsigned u;
    for(u = 0; u < UINT_MAX; u++) {
        if(u % 500000000u == 0u) printf("%u\n", u);
        do_test(&u);
    }
    do_test(&u);

    puts("signed");
    int i;
    for(i = INT_MIN; i < INT_MAX; i++) {
        if(i % 500000000 == 0) printf("%d\n", i);
        do_test(&i);
    }
    do_test(&i);
}

