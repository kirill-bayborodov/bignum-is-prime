#include "bignum_is_prime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint64_t next_value(uint64_t *state)
{
    *state ^= *state << 7U; *state ^= *state >> 9U; *state ^= *state << 8U; return *state;
}

static int reference_u64(uint64_t number)
{
    if (number < 2U) return 0;
    for (uint64_t divisor = 2U; divisor <= number / divisor; ++divisor) {
        if (number % divisor == 0U) return number == divisor;
    }
    return 1;
}

static void test_randomized_small_values(void)
{
    uint64_t state = UINT64_C(0x123456789abcdef);
    for (size_t index = 0U; index < 2000U; ++index) {
        const uint64_t number_value = next_value(&state) % 100000U;
        bignum_t number = { .words = { number_value }, .len = number_value == 0U ? 0U : 1U };
        int result = -1;
        assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
        assert(result == reference_u64(number_value));
    }
}

static void test_normalization_and_guards(void)
{
    bignum_t number = {0};
    uint64_t guard_before = UINT64_C(0xDEADBEEFDEADBEEF);
    uint64_t guard_after = UINT64_C(0xA5A5A5A5A5A5A5A5);
    int result = -1;
    number.words[0] = 97U; number.len = BIGNUM_CAPACITY;
    assert(bignum_is_prime(&number, 8U, &result) == BIGNUM_IS_PRIME_SUCCESS);
    assert(result == 1);
    assert(guard_before == UINT64_C(0xDEADBEEFDEADBEEF));
    assert(guard_after == UINT64_C(0xA5A5A5A5A5A5A5A5));
}

int main(void)
{
    test_randomized_small_values();
    test_normalization_and_guards();
    puts("bignum_is_prime extra tests: OK");
    return 0;
}
