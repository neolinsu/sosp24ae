#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <mt19937_64.h>


#include <base/assert.h>

int main() {
    mt19937_64_init();
    uint64_t r64 = genrand64_int64();
    assert(r64==7266447313870364031llu); // Predictable
    puts("SUCCESS");
    return 0;
}