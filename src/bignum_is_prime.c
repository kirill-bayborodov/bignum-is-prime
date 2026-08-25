/**
 * @file bignum_is_prime.c
 * @brief C11 reference implementation of Miller–Rabin primality testing.
 * @details
 * The implementation uses fixed-capacity little-endian words and bounded
 * modular arithmetic. It is intentionally a correctness baseline; the later
 * assembly implementation is measured against this implementation's tests and
 * benchmark protocol. No input is modified and no mutable global state exists.
 */
#include "bignum_is_prime.h"

#include <stdint.h>
#include <string.h>

#define PRIME_WORD_BITS 64U
#define PRIME_BASE_COUNT 12U

/** @brief Compares two normalized non-negative bignum values. */
static int compare(const bignum_t *left, const bignum_t *right)
{
    if (left->len != right->len) return left->len < right->len ? -1 : 1;
    for (size_t index = left->len; index > 0U; --index) {
        if (left->words[index - 1U] != right->words[index - 1U]) {
            return left->words[index - 1U] < right->words[index - 1U] ? -1 : 1;
        }
    }
    return 0;
}

/** @brief Removes zero words above the normalized most-significant word. */
static void normalize(bignum_t *value)
{
    while (value->len > 0U && value->words[value->len - 1U] == 0U) --value->len;
}

/** @brief Reports whether a bignum value is zero. */
static int is_zero(const bignum_t *value)
{
    return value->len == 0U;
}

/** @brief Sets a bignum value to one. */
static void set_one(bignum_t *value)
{
    memset(value, 0, sizeof(*value));
    value->words[0] = 1U;
    value->len = 1U;
}

/** @brief Sets a bignum value from a 64-bit word. */
static void set_u64(bignum_t *value, uint64_t word)
{
    memset(value, 0, sizeof(*value));
    if (word != 0U) {
        value->words[0] = word;
        value->len = 1U;
    }
}

/** @brief Subtracts right from left when left is greater than or equal to right. */
static void subtract(const bignum_t *left, const bignum_t *right, bignum_t *result)
{
    uint64_t borrow = 0U;
    memset(result, 0, sizeof(*result));
    result->len = left->len;
    for (size_t index = 0U; index < left->len; ++index) {
        const uint64_t subtrahend = (index < right->len ? right->words[index] : 0U);
        const uint64_t first = left->words[index] - subtrahend;
        const uint64_t first_borrow = left->words[index] < subtrahend;
        const uint64_t second = first - borrow;
        const uint64_t second_borrow = first < borrow;
        result->words[index] = second;
        borrow = first_borrow | second_borrow;
    }
    normalize(result);
}

/** @brief Doubles a modular value and reduces it without overflowing. */
static void double_mod(const bignum_t *value, const bignum_t *modulus,
                       bignum_t *result)
{
    uint64_t carry = 0U;
    memset(result, 0, sizeof(*result));
    result->len = value->len;
    for (size_t index = 0U; index < value->len; ++index) {
        const uint64_t word = value->words[index];
        result->words[index] = (word << 1U) | carry;
        carry = word >> 63U;
    }
    if (carry != 0U) {
        bignum_t difference;
        subtract(modulus, value, &difference);
        subtract(value, &difference, result);
    } else if (compare(result, modulus) >= 0) {
        bignum_t reduced;
        subtract(result, modulus, &reduced);
        *result = reduced;
    }
}

/** @brief Adds two modular values and reduces the bounded sum. */
static void add_mod(const bignum_t *left, const bignum_t *right,
                    const bignum_t *modulus, bignum_t *result)
{
    bignum_t complement;
    if (compare(modulus, right) >= 0) {
        subtract(modulus, right, &complement);
        if (compare(left, &complement) >= 0) {
            subtract(left, &complement, result);
            return;
        }
    }
    memset(result, 0, sizeof(*result));
    result->len = left->len > right->len ? left->len : right->len;
    uint64_t carry = 0U;
    for (size_t index = 0U; index < result->len; ++index) {
        const uint64_t a = index < left->len ? left->words[index] : 0U;
        const uint64_t b = index < right->len ? right->words[index] : 0U;
        const uint64_t sum = a + b + carry;
        carry = (sum < a) || (carry != 0U && sum == a);
        result->words[index] = sum;
    }
}

/** @brief Multiplies two modular values with binary double-and-add. */
static void multiply_mod(const bignum_t *left, const bignum_t *right,
                         const bignum_t *modulus, bignum_t *result)
{
    bignum_t accumulator;
    bignum_t addend = *left;
    memset(&accumulator, 0, sizeof(accumulator));
    for (size_t word = 0U; word < right->len; ++word) {
        uint64_t bits = right->words[word];
        for (size_t bit = 0U; bit < PRIME_WORD_BITS; ++bit) {
            if ((bits & 1U) != 0U) {
                bignum_t sum;
                add_mod(&accumulator, &addend, modulus, &sum);
                accumulator = sum;
            }
            bits >>= 1U;
            double_mod(&addend, modulus, &addend);
        }
    }
    *result = accumulator;
}

/** @brief Divides a bignum by two in place and normalizes it. */
static void halve(bignum_t *value)
{
    uint64_t carry = 0U;
    for (size_t index = value->len; index > 0U; --index) {
        const uint64_t word = value->words[index - 1U];
        value->words[index - 1U] = (word >> 1U) | (carry << 63U);
        carry = word & 1U;
    }
    normalize(value);
}

/** @brief Computes base^exponent modulo modulus by square-and-multiply. */
static void power_mod(const bignum_t *base, const bignum_t *exponent,
                      const bignum_t *modulus, bignum_t *result)
{
    bignum_t accumulator;
    bignum_t factor = *base;
    set_one(&accumulator);
    if (compare(&accumulator, modulus) >= 0) memset(&accumulator, 0, sizeof(accumulator));
    for (size_t word = 0U; word < exponent->len; ++word) {
        uint64_t bits = exponent->words[word];
        for (size_t bit = 0U; bit < PRIME_WORD_BITS; ++bit) {
            if ((bits & 1U) != 0U) {
                bignum_t product;
                multiply_mod(&accumulator, &factor, modulus, &product);
                accumulator = product;
            }
            bits >>= 1U;
            if (word + 1U < exponent->len || bits != 0U) {
                bignum_t square;
                multiply_mod(&factor, &factor, modulus, &square);
                factor = square;
            }
        }
    }
    *result = accumulator;
}

/** @brief Computes a small-prime remainder without wide integer extensions. */
static unsigned remainder_u64(const bignum_t *value, unsigned divisor)
{
    unsigned remainder = 0U;
    for (size_t word = value->len; word > 0U; --word) {
        uint64_t bits = value->words[word - 1U];
        for (size_t bit = 0U; bit < PRIME_WORD_BITS; ++bit) {
            remainder = (remainder * 2U + (unsigned)(bits >> 63U)) % divisor;
            bits <<= 1U;
        }
    }
    return remainder;
}

__extension__ typedef unsigned __int128 prime_u128_t;

/** @brief Multiplies two 64-bit values modulo a 64-bit modulus. */
static uint64_t mulmod_u64(uint64_t left, uint64_t right, uint64_t modulus)
{
    return (uint64_t)(((prime_u128_t)left * (prime_u128_t)right) % modulus);
}

/** @brief Raises a 64-bit value to a 64-bit exponent modulo a modulus. */
static uint64_t powmod_u64(uint64_t base, uint64_t exponent, uint64_t modulus)
{
    uint64_t result = 1U % modulus;
    base %= modulus;
    while (exponent != 0U) {
        if ((exponent & 1U) != 0U) result = mulmod_u64(result, base, modulus);
        base = mulmod_u64(base, base, modulus);
        exponent >>= 1U;
    }
    return result;
}

/** @brief Performs deterministic Miller–Rabin for a 64-bit input. */
static int is_prime_u64(uint64_t number)
{
    static const uint64_t bases[] = { 2U, 325U, 9375U, 28178U, 450775U, 9780504U, 1795265022U };
    uint64_t odd = number - 1U;
    unsigned powers = 0U;
    while ((odd & 1U) == 0U) { odd >>= 1U; ++powers; }
    for (size_t index = 0U; index < sizeof(bases) / sizeof(bases[0]); ++index) {
        uint64_t value = powmod_u64(bases[index], odd, number);
        if (value == 1U || value == number - 1U) continue;
        unsigned power;
        for (power = 1U; power < powers; ++power) {
            value = mulmod_u64(value, value, number);
            if (value == number - 1U) break;
        }
        if (power == powers) return 0;
    }
    return 1;
}

/** @brief Runs one Miller–Rabin witness round. */
static int witness(const bignum_t *number, const bignum_t *exponent,
                   size_t powers, unsigned base)
{
    bignum_t base_value;
    bignum_t value;
    bignum_t minus_one;
    set_u64(&base_value, (uint64_t)base);
    power_mod(&base_value, exponent, number, &value);
    subtract(number, &(bignum_t){ .words = { 1U }, .len = 1U }, &minus_one);
    if (compare(&value, &(bignum_t){ .words = { 1U }, .len = 1U }) == 0 ||
        compare(&value, &minus_one) == 0) return 1;
    for (size_t count = 1U; count < powers; ++count) {
        bignum_t square;
        multiply_mod(&value, &value, number, &square);
        value = square;
        if (compare(&value, &minus_one) == 0) return 1;
    }
    return 0;
}

bignum_is_prime_status_t bignum_is_prime(const bignum_t *num, size_t rounds,
                                         int *is_prime)
{
    static const unsigned bases[PRIME_BASE_COUNT] = { 2U, 3U, 5U, 7U, 11U, 13U,
                                                       17U, 19U, 23U, 29U, 31U, 37U };
    bignum_t number;
    bignum_t exponent;
    size_t powers = 0U;

    if (num == NULL || is_prime == NULL) return BIGNUM_IS_PRIME_ERROR_NULL_ARG;
    if (num->len > BIGNUM_CAPACITY) return BIGNUM_IS_PRIME_ERROR_BAD_LENGTH;
    if (rounds == 0U) return BIGNUM_IS_PRIME_ERROR_ROUNDS;
    number = *num;
    normalize(&number);
    *is_prime = 0;
    if (number.len == 0U || (number.len == 1U && number.words[0] < 2U)) return BIGNUM_IS_PRIME_SUCCESS;
    if (number.len == 1U && number.words[0] <= 37U) {
        static const unsigned small_primes[] = { 2U, 3U, 5U, 7U, 11U, 13U, 17U, 19U, 23U, 29U, 31U, 37U };
        *is_prime = 0;
        for (size_t index = 0U; index < sizeof(small_primes) / sizeof(small_primes[0]); ++index) {
            if (number.words[0] == small_primes[index]) *is_prime = 1;
        }
        return BIGNUM_IS_PRIME_SUCCESS;
    }
    if ((number.words[0] & 1U) == 0U) return BIGNUM_IS_PRIME_SUCCESS;
    for (unsigned divisor = 3U; divisor <= 37U; divisor += 2U) {
        if (remainder_u64(&number, divisor) == 0U) return BIGNUM_IS_PRIME_SUCCESS;
    }
    if (number.len == 1U) {
        *is_prime = is_prime_u64(number.words[0]);
        return BIGNUM_IS_PRIME_SUCCESS;
    }
    exponent = number;
    {
        bignum_t one;
        set_one(&one);
        subtract(&exponent, &one, &exponent);
    }
    while (!is_zero(&exponent) && (exponent.words[0] & 1U) == 0U) {
        halve(&exponent);
        ++powers;
    }
    for (size_t round = 0U; round < rounds; ++round) {
        const unsigned base = bases[round % PRIME_BASE_COUNT];
        if ((number.len == 1U && number.words[0] <= base) ||
            !witness(&number, &exponent, powers, base)) return BIGNUM_IS_PRIME_SUCCESS;
    }
    *is_prime = 1;
    return BIGNUM_IS_PRIME_SUCCESS;
}
