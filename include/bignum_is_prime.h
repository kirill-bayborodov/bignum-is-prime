/**
 * @file bignum_is_prime.h
 * @brief Public API for Miller--Rabin primality testing of bignum_t values.
 * @details
 * The module accepts a caller-owned, non-negative fixed-capacity bignum_t and
 * writes a Boolean probable-primality result to caller-owned storage. No
 * allocation, ownership transfer, input mutation, or mutable global state is
 * used. Independent calls are reentrant and safe to execute concurrently.
 */
#ifndef BIGNUM_IS_PRIME_H
#define BIGNUM_IS_PRIME_H

#include <bignum.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reports validation and execution outcomes of bignum_is_prime.
 * @details
 * A success status guarantees that the output pointer was written with either
 * zero or one. Every error status leaves the caller-owned output unchanged;
 * errors are deterministic and may be retried after the input contract is
 * corrected. No status indicates allocation or ownership transfer.
 */
typedef enum bignum_is_prime_status {
    BIGNUM_IS_PRIME_SUCCESS = 0, /**< [out] Predicate completed; output is 0 or 1 and inputs remain unchanged. */
    BIGNUM_IS_PRIME_ERROR_NULL_ARG = -1, /**< [out] num or is_prime is NULL; no output can be written; correct the pointer and retry. */
    BIGNUM_IS_PRIME_ERROR_BAD_LENGTH = -2, /**< [out] num->len exceeds BIGNUM_CAPACITY; output is unchanged; repair the record and retry. */
    BIGNUM_IS_PRIME_ERROR_ROUNDS = -3 /**< [out] rounds is zero; output is unchanged; provide a positive round count and retry. */
} bignum_is_prime_status_t;

/**
 * @brief Tests whether a bignum_t is probably prime.
 * @details
 * The function copies and normalizes the input, rejects values below two,
 * even values, and small-prime divisors, then performs square-and-multiply
 * Miller--Rabin rounds using fixed bases. One-word values use deterministic
 * 64-bit bases; larger values use bounded fixed-capacity arithmetic. The
 * result is probabilistic for general multiword inputs and becomes stronger
 * as the positive round count increases. Time is O(rounds * n^3) for the
 * bounded binary multiply path and space is O(n), where n is word count.
 * @param[in] num Borrowed caller-owned input record; non-NULL, non-negative,
 *                and len must be in [0, BIGNUM_CAPACITY]. The record is not
 *                modified and may alias no output storage.
 * @param[in] rounds Positive number of Miller--Rabin rounds; units are rounds.
 *                   The caller owns this scalar and retains it after return.
 * @param[out] is_prime Caller-allocated int receiving 1 for probable prime or
 *                       0 for composite only on BIGNUM_IS_PRIME_SUCCESS. The
 *                       pointed storage is borrowed and unchanged on errors.
 * @return BIGNUM_IS_PRIME_SUCCESS, BIGNUM_IS_PRIME_ERROR_NULL_ARG,
 *         BIGNUM_IS_PRIME_ERROR_BAD_LENGTH, or BIGNUM_IS_PRIME_ERROR_ROUNDS.
 * @pre num and is_prime are non-NULL; num->len does not exceed capacity;
 *      rounds is positive; caller retains both objects for the call duration.
 * @post On success, *is_prime is exactly zero or one and num is byte-for-byte
 *       unchanged. On error, *is_prime is unchanged.
 * @warning The predicate reports probable-prime for general multiword input;
 *          it is not a proof of primality. The fixed-capacity representation
 *          does not permit values with len greater than BIGNUM_CAPACITY.
 * @thread_safety Safe for concurrent calls when each call's output storage is
 *                independent; the input is borrowed read-only.
 */
bignum_is_prime_status_t bignum_is_prime(
    const bignum_t *num,
    size_t rounds,
    int *is_prime);

#ifdef __cplusplus
}
#endif

#endif /* BIGNUM_IS_PRIME_H */
