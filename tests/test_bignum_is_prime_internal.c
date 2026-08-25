/**
 * @file test_bignum_is_prime_internal.c
 * @brief Direct deterministic tests for bounded C11 arithmetic helpers.
 * @details
 * This test translation unit includes the reference source with its public
 * entry point renamed. It exercises helper invariants that are otherwise hard
 * to reach through the public predicate: carry reduction, modular addition and
 * multiplication, halving, square-and-multiply, and both witness outcomes.
 * The production public symbol remains supplied by the normal object file.
 */
#define static
#define bignum_is_prime bignum_is_prime_internal_for_test
#include "../src/bignum_is_prime.c"
#undef bignum_is_prime
#undef static
#include <assert.h>
#include <stdio.h>

/** @brief Builds a one-word temporary value. */
static bignum_t word(uint64_t value)
{
    bignum_t result = { .words = { value }, .len = value == 0U ? 0U : 1U };
    return result;
}

/** @brief Exercises compare, subtraction, modular double/add/multiply and halving. */
static void test_arithmetic_helpers(void)
{
    bignum_t left = word(10U), right = word(9U), modulus = word(17U), result;
    assert(compare(&left, &right) > 0);
    subtract(&left, &right, &result);
    assert(result.words[0] == 1U && result.len == 1U);
    double_mod(&right, &modulus, &result);
    assert(result.words[0] == 1U);
    add_mod(&left, &right, &modulus, &result);
    assert(result.words[0] == 2U);
    multiply_mod(&left, &right, &modulus, &result);
    assert(result.words[0] == 5U);
    halve(&left);
    assert(left.words[0] == 5U);

    left = word(UINT64_C(1) << 63U);
    right = word(UINT64_C(1) << 63U);
    modulus = word(UINT64_MAX - 58U);
    double_mod(&left, &modulus, &result);
    assert(result.words[0] == 59U);
    add_mod(&left, &right, &modulus, &result);
    assert(result.words[0] == 59U);
}

/** @brief Exercises square-and-multiply and explicit witness pass/fail paths. */
static void test_power_and_witness(void)
{
    bignum_t base = word(5U), exponent = word(13U), modulus = word(17U), result;
    power_mod(&base, &exponent, &modulus, &result);
    assert(result.words[0] == 3U);
    exponent = word(7U);
    modulus = word(15U);
    assert(witness(&modulus, &exponent, 0U, 2U) == 0);
    exponent = word(1U);
    modulus = word(17U);
    assert(witness(&modulus, &exponent, 4U, 3U) == 1);
}

/** @brief Runs helper-level deterministic coverage scenarios. */
int main(void)
{
    test_arithmetic_helpers();
    test_power_and_witness();
    puts("bignum_is_prime internal tests: OK");
    return 0;
}
