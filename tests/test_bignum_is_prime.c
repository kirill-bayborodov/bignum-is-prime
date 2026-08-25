#include "bignum_is_prime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bignum_t make_u64(uint64_t word)
{
    bignum_t number = {0};
    number.words[0] = word;
    number.len = word == 0U ? 0U : 1U;
    return number;
}

static void expect_u64(uint64_t word, int expected)
{
    bignum_t number = make_u64(word);
    bignum_t original = number;
    int result = -1;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == expected);
    assert(memcmp(&number, &original, sizeof(number)) == 0);
}

static void test_small_values(void)
{
    for (uint64_t number = 0U; number < 4U; ++number) {
        expect_u64(number, number == 2U || number == 3U);
    }
    expect_u64(5U, 1); expect_u64(7U, 1); expect_u64(11U, 1);
    expect_u64(13U, 1); expect_u64(17U, 1); expect_u64(19U, 1);
    expect_u64(4U, 0); expect_u64(9U, 0); expect_u64(15U, 0);
    expect_u64(21U, 0); expect_u64(25U, 0); expect_u64(49U, 0);
}

static void test_large_words(void)
{
    expect_u64(UINT64_C(18446744073709551557), 1);
    expect_u64(UINT64_C(18446744073709551555), 0);
}

static void test_multword(void)
{
    bignum_t number = {0};
    int result = -1;
    number.words[0] = 1U;
    number.len = 1U;
    assert(bignum_is_prime(&number, 4U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == 0);
    number.words[0] = 3U;
    number.words[1] = 1U;
    number.len = 2U;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == 1);
    number.words[0] = 5U;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == 0);
}

static void test_contract(void)
{
    bignum_t number = make_u64(17U);
    int result = 77;
    assert(bignum_is_prime(NULL, 8U, &result) == BIGNUM_IS_PRIME_ERROR_NULL_ARG);
    assert(bignum_is_prime(&number, 8U, NULL) == BIGNUM_IS_PRIME_ERROR_NULL_ARG);
    assert(bignum_is_prime(&number, 0U, &result) == BIGNUM_IS_PRIME_ERROR_ROUNDS);
    number.len = BIGNUM_CAPACITY + 1U;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_ERROR_BAD_LENGTH);
    assert(result == 77);
}

int main(void)
{
    test_small_values();
    test_large_words();
    test_multword();
    test_contract();
    puts("bignum_is_prime deterministic tests: OK");
    return 0;
}
