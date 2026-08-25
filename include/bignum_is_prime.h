/**
 * @file bignum_is_prime.h
 * @brief Public API for Miller–Rabin primality testing of bignum_t values.
 *
 * @details
 * The operation reads a normalized non-negative bignum_t and writes a boolean
 * primality result to a caller-owned integer. It performs the requested number
 * of deterministic-base Miller–Rabin rounds. The input object is never modified.
 * All temporary values use fixed-size automatic storage, so independent calls
 * are reentrant and thread-safe.
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
 */
typedef enum bignum_is_prime_status {
    BIGNUM_IS_PRIME_SUCCESS = 0,
    BIGNUM_IS_PRIME_ERROR_NULL_ARG = -1,
    BIGNUM_IS_PRIME_ERROR_BAD_LENGTH = -2,
    BIGNUM_IS_PRIME_ERROR_ROUNDS = -3
} bignum_is_prime_status_t;

/**
 * @brief Tests whether a bignum_t is probably prime.
 * @param[in] num Normalized non-negative input; it is never modified.
 * @param[in] rounds Positive number of Miller–Rabin rounds.
 * @param[out] is_prime Receives 1 for probable prime and 0 for composite.
 * @return A named bignum_is_prime_status_t value.
 * @details
 * Values below two are composite. Small prime divisors are rejected before
 * Miller–Rabin rounds. Each round uses a fixed base selected cyclically from
 * the deterministic base set. Increase rounds when a stronger probabilistic
 * bound is required. On every error `*is_prime` is unchanged.
 * @thread_safety Safe for concurrent calls because no mutable global state is used.
 */
bignum_is_prime_status_t bignum_is_prime(
    const bignum_t *num,
    size_t rounds,
    int *is_prime);

#ifdef __cplusplus
}
#endif

#endif /* BIGNUM_IS_PRIME_H */
