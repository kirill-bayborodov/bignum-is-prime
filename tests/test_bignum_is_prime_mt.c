/**
 * @file test_bignum_is_prime_mt.c
 * @brief Concurrent read-only contract test for bignum_is_prime.
 * @details
 * Eight pthread workers each perform 100 calls on an independent canonical
 * record containing 1009. Every call must return the named success status and
 * probable-prime result. The test checks reentrancy and absence of mutable
 * shared state; pthread_join is required before process success is published.
 */
#include "bignum_is_prime.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>

#define THREAD_COUNT 8U
#define ITERATIONS 100U

/** @brief Owns one worker's borrowed input and observed result. */
typedef struct worker_data {
    bignum_t number; /**< [in] Independent immutable operand owned by this worker. */
    int expected;    /**< [in] Expected predicate result, either zero or one. */
    int result;      /**< [out] Last result written by the library call. */
} worker_data_t;

/** @brief Repeats the named-status and predicate assertions in one worker. */
static void *worker(void *argument)
{
    worker_data_t *data = argument;
    for (size_t index = 0U; index < ITERATIONS; ++index) {
        data->result = -1;
        assert(bignum_is_prime(&data->number, 8U, &data->result) == BIGNUM_IS_PRIME_SUCCESS);
        assert(data->result == data->expected);
    }
    return NULL;
}

/** @brief Creates, joins, and validates all independent worker executions. */
int main(void)
{
    pthread_t threads[THREAD_COUNT];
    worker_data_t data[THREAD_COUNT];
    for (size_t index = 0U; index < THREAD_COUNT; ++index) {
        data[index] = (worker_data_t){ .number = { .words = { 1009U }, .len = 1U },
                                       .expected = 1, .result = -1 };
        assert(pthread_create(&threads[index], NULL, worker, &data[index]) == 0);
    }
    for (size_t index = 0U; index < THREAD_COUNT; ++index) {
        assert(pthread_join(threads[index], NULL) == 0);
    }
    puts("bignum_is_prime MT tests: OK");
    return 0;
}
