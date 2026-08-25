#include "bignum_is_prime.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    bignum_t number = { .words = { 97U }, .len = 1U };
    int result = 0;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == 1);
    puts("bignum_is_prime runner: OK");
    return 0;
}
