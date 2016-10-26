#define THREAD 8
#define QUEUE  256
#define DATASIZE (20000)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "threadpool.h"
#include "yastopwatch.h"

DEF_THREADED_SW(simple_time)

void is_simple(int n, void *_data)
{
    START_SW(simple_time);

    int *data = (int *) _data;
    int x = data[n];
    data[n] = 0;
    if (x < 2) return;
    if (x > 2 && x % 2 == 0) return;
    for (int i = 3; i * i <= x; i += 2)
        if (x % i == 0) return;
    data[n] = x;

    STOP_SW(simple_time);
    SYNC_SW(simple_time);
}

void my_reduce(void *self, void *left, void *right)
{
    *((int *) left) += *((int *) right);
}

void *my_alloc_neutral(void *self)
{
    int *c = malloc(sizeof(int));
    *c = 0;
    return c;
}

void my_free(void *self, void *node)
{
    free(node);
}

void my_finish(void *self, void *node)
{
    printf("reduce result = %d\n", *(int *) node);
}

int main(int argc, char *argv[])
{
    DEF_SW(map_time);
    DEF_SW(reduce_time);
    threadpool_t *pool;

    pool = threadpool_create(THREAD, QUEUE, 0);
    fprintf(stderr, "Pool started with %d threads and "
            "queue size of %d\n", THREAD, QUEUE);

    int *data = malloc(DATASIZE * sizeof(int));
    for (int i = 0; i < DATASIZE; i++)
        data[i] = i + 1;

    START_SW(map_time);

    threadpool_map(pool, DATASIZE, is_simple, data, 0);

    STOP_SW(map_time);

    for (int i = 0; i < DATASIZE; i++)
        printf("%c", !!data[i] ? '-' : ' ');
    printf("\n");

    START_SW(reduce_time);

    threadpool_reduce_t reduce = {
        .begin = data,
        .end = data + DATASIZE,
        .object_size = sizeof(*data),
        .self = NULL,
        .reduce = my_reduce,
        .reduce_alloc_neutral = my_alloc_neutral,
        .reduce_finish = my_finish,
        .reduce_free = my_free,
    };

    threadpool_reduce(pool, &reduce);

    STOP_SW(reduce_time);

    fprintf(stderr, "[map] Total time: %lf\n", GET_SEC(map_time));
    fprintf(stderr, "[is_simple] Total time: %lf\n", GET_SEC(simple_time));
    fprintf(stderr, "[reduce] Total time: %lf\n", GET_SEC(reduce_time));

    return 0;
}
