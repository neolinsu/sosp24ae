#include <stdio.h>
#include <stdlib.h>
#include <rand.h>

#define ROUNDS 10000000
#define EPS 0.001


#include <base/assert.h>

int main () {
    double mean = 0.5; // E(x) = 1/mean
    double acc = 0;
    for (int i=0; i<ROUNDS; i++) {
        acc += gen_exponential(mean); 
    }
    acc /= ROUNDS;
    assert((acc-1/mean)/(1/mean)<EPS);
    puts("SUCCESS");
    return 0;
}