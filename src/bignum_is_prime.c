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

/**
 * @brief Compares two normalized non-negative bignum values.
 * @details Inputs are borrowed and unchanged; length and words are compared
 * from most significant to least significant. Complexity is O(n).
 * @param[in] left Normalized borrowed value.
 * @param[in] right Normalized borrowed value.
 * @return Negative, zero, or positive according to left/right ordering.
 */
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

/**
 * @brief Removes zero words above the normalized most-significant word.
 * @details This mutates only the caller-owned temporary value and preserves all
 * significant words. Complexity is O(n) in the number of removed words.
 * @param[in,out] value Caller-owned temporary with len in capacity.
 */
static void normalize(bignum_t *value)
{
    while (value->len > 0U && value->words[value->len - 1U] == 0U) --value->len;
}

/**
 * @brief Reports whether a normalized bignum value is zero.
 * @param[in] value Borrowed normalized value; it is unchanged.
 * @return Non-zero when len is zero, otherwise zero.
 */
static int is_zero(const bignum_t *value)
{
    return value->len == 0U;
}

/**
 * @brief Sets a caller-owned bignum temporary to one.
 * @details The complete fixed-capacity record is cleared before publication.
 * @param[out] value Caller-owned output record; non-NULL and writable.
 */
static void set_one(bignum_t *value)
{
    memset(value, 0, sizeof(*value));
    value->words[0] = 1U;
    value->len = 1U;
}

/**
 * @brief Sets a caller-owned bignum temporary from one 64-bit word.
 * @details Zero is represented canonically with len zero.
 * @param[out] value Caller-owned output record; non-NULL and writable.
 * @param[in] word Unsigned source word; no ownership is transferred.
 */
static void set_u64(bignum_t *value, uint64_t word)
{
    memset(value, 0, sizeof(*value));
    if (word != 0U) {
        value->words[0] = word;
        value->len = 1U;
    }
}

/**
 * @brief Subtracts right from left under the caller-checked ordering invariant.
 * @details The routine uses bounded borrow arithmetic, normalizes the result,
 * and assumes left is at least right; violating that invariant is a bug.
 * @param[in] left Borrowed minuend, unchanged.
 * @param[in] right Borrowed subtrahend no greater than left, unchanged.
 * @param[out] result Caller-owned non-aliasing output record.
 */
static void subtract(const bignum_t *left, const bignum_t *right, bignum_t *result)
{
    const bignum_t minuend = *left;
    const bignum_t subtrahend_value = *right;
    uint64_t borrow = 0U;
    memset(result, 0, sizeof(*result));
    result->len = minuend.len;
    for (size_t index = 0U; index < minuend.len; ++index) {
        const uint64_t subtrahend = (index < subtrahend_value.len ? subtrahend_value.words[index] : 0U);
        const uint64_t first = minuend.words[index] - subtrahend;
        const uint64_t first_borrow = minuend.words[index] < subtrahend;
        const uint64_t second = first - borrow;
        const uint64_t second_borrow = first < borrow;
        result->words[index] = second;
        borrow = first_borrow | second_borrow;
    }
    normalize(result);
}

/**
 * @brief Doubles a modular value and reduces it without overflowing.
 * @details The carry branch computes x-(modulus-x), preserving the mathematical
 * result when the fixed word array overflows. Inputs are reduced and result is
 * written atomically after bounded arithmetic. Complexity is O(n).
 * @param[in] value Borrowed value in [0, modulus).
 * @param[in] modulus Borrowed positive modulus.
 * @param[out] result Caller-owned output; it may alias value.
 */
static void double_mod(const bignum_t *value, const bignum_t *modulus,
                       bignum_t *result)
{
    const bignum_t input = *value;
    uint64_t carry = 0U;
    memset(result, 0, sizeof(*result));
    result->len = input.len;
    for (size_t index = 0U; index < input.len; ++index) {
        const uint64_t word = input.words[index];
        result->words[index] = (word << 1U) | carry;
        carry = word >> 63U;
    }
    if (carry != 0U) {
        bignum_t difference;
        subtract(modulus, &input, &difference);
        subtract(&input, &difference, result);
    } else if (compare(result, modulus) >= 0) {
        bignum_t reduced;
        subtract(result, modulus, &reduced);
        *result = reduced;
    }
}

/**
 * @brief Adds two modular values and reduces the bounded sum.
 * @details Complement comparison avoids a carry beyond fixed capacity; inputs
 * must be below modulus. Complexity is O(n).
 * @param[in] left Borrowed reduced addend.
 * @param[in] right Borrowed reduced addend.
 * @param[in] modulus Borrowed positive modulus.
 * @param[out] result Caller-owned output record.
 */
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

/**
 * @brief Multiplies two modular values with binary double-and-add.
 * @details Every set bit adds the current modular addend and every processed bit
 * doubles it. Inputs are borrowed; result may be published after O(n^3) work.
 * @param[in] left Borrowed reduced multiplicand.
 * @param[in] right Borrowed reduced multiplier.
 * @param[in] modulus Borrowed positive modulus.
 * @param[out] result Caller-owned output record.
 */
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

/**
 * @brief Divides a bignum by two in place and normalizes it.
 * @details Words are traversed most-significant first so carry represents the
 * preceding word's low bit. Complexity is O(n).
 * @param[in,out] value Caller-owned temporary representing a non-negative value.
 */
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

/**
 * @brief Computes base to exponent modulo modulus by square-and-multiply.
 * @details Bits are consumed least-significant first; accumulator and factor
 * remain reduced after every operation. Complexity is O(n^3 * bit_length).
 * @param[in] base Borrowed reduced or unreduced base.
 * @param[in] exponent Borrowed non-negative exponent.
 * @param[in] modulus Borrowed positive modulus.
 * @param[out] result Caller-owned output record.
 */
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

/**
 * @brief Computes a small-prime remainder without wide integer extensions.
 * @details The fixed bit scan is portable C11 and uses constant extra space.
 * Complexity is O(64n).
 * @param[in] value Borrowed non-negative value, unchanged.
 * @param[in] divisor Positive small divisor.
 * @return value modulo divisor.
 */
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

/**
 * @brief Multiplies two 64-bit values modulo a 64-bit modulus.
 * @param[in] left Unsigned operand below modulus.
 * @param[in] right Unsigned operand below modulus.
 * @param[in] modulus Positive 64-bit modulus.
 * @return Product reduced modulo modulus.
 */
static uint64_t mulmod_u64(uint64_t left, uint64_t right, uint64_t modulus)
{
    return (uint64_t)(((prime_u128_t)left * (prime_u128_t)right) % modulus);
}

/**
 * @brief Raises a 64-bit value to a 64-bit exponent modulo a modulus.
 * @details Uses binary exponentiation and constant temporary storage.
 * @param[in] base Unsigned base.
 * @param[in] exponent Unsigned exponent.
 * @param[in] modulus Positive modulus.
 * @return Modular power result.
 */
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

/**
 * @brief Performs deterministic Miller--Rabin for a 64-bit input.
 * @details Uses the seven-base deterministic set valid for the uint64 domain.
 * @param[in] number Candidate greater than one.
 * @return Non-zero for prime, zero for composite.
 */
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

/**
 * @brief Runs one Miller--Rabin witness round.
 * @details A witness returning zero proves compositeness; a one means this base
 * did not disprove probable primality. All temporaries are stack-owned.
 * @param[in] number Borrowed odd candidate.
 * @param[in] exponent Odd component of number-1.
 * @param[in] powers Number of powers of two removed from number-1.
 * @param[in] base Fixed witness base.
 * @return Non-zero when the candidate passes this witness, otherwise zero.
 */
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
