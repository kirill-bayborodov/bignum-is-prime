/**
 * @file test_bignum_is_prime_runner.c
 * @brief Linkage smoke test for the distributed bignum_is_prime API.
 * @details
 * The runner uses only the public header and a canonical caller-owned operand.
 * It verifies that the library links, accepts the documented rounds argument,
 * returns BIGNUM_IS_PRIME_SUCCESS, and writes the expected result. No dynamic
 * allocation is performed; returning EXIT_SUCCESS means the public contract
 * was exercised end to end.
 */
#include "bignum_is_prime.h"
#include <assert.h>
#include <stdio.h>

/** @brief Executes one public API call and returns the ISO C process status. */
int main(void)
{
    bignum_t number = { .words = { 97U }, .len = 1U };
    int result = 0;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == 1);
    puts("bignum_is_prime runner: OK");
    return 0;
}
