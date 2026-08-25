#include "bignum_is_prime.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define THREAD_COUNT 8U
#define ITERATIONS 100U

typedef struct {
    bignum_t number;
    int expected;
    int result;
} worker_data_t;

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

int main(void)
{
    pthread_t threads[THREAD_COUNT];
    worker_data_t data[THREAD_COUNT];
    for (size_t index = 0U; index < THREAD_COUNT; ++index) {
        data[index] = (worker_data_t){ .number = { .words = { 1009U }, .len = 1U },
                                       .expected = 1, .result = -1 };
        assert(pthread_create(&threads[index], NULL, worker, &data[index]) == 0);
    }
    for (size_t index = 0U; index < THREAD_COUNT; ++index) assert(pthread_join(threads[index], NULL) == 0);
    puts("bignum_is_prime MT tests: OK");
    return 0;
}
