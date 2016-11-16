#define THREAD 8
#define QUEUE 256
#define DATASIZE (2000000)

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include "threadpool.h"

#ifdef PROFILE
#include "yastopwatch.h"

DEF_THREADED_SW(simple_time)
DEF_SW(map_time)
DEF_SW(reduce_time)
#endif

void is_simple(int n, void *_data)
{
#ifdef PROFILE
    START_SW(simple_time);
#endif

    long *data = (long *) _data;
    long x = data[n];
    data[n] = 0;
    if (x < 2) return;
    if ((x & ~3) && !(x & 1)) return;
    for (int i = 3; i * i <= x; i += 2)
        if (x % i == 0) return;
    data[n] = x;

#ifdef PROFILE
    STOP_SW(simple_time);
    SYNC_SW(simple_time);
#endif
}

void my_reduce(void *additional, void *result, void *data)
{
    *((long *) result) += *((long *) data);
}

void my_merge(void *additional , void *result , void *local)
{
    *((long *) result) += *((long *) local); 
}

// FIXME: should be provided default by library
// allocate 'local' space for reduce phase
void *my_alloc_neutral(void *additional)
{
    int *c = malloc(sizeof(long));
    *c = 0;
    return c;
}

// FIXME: should be provided default by library
// managed by library
void my_free(void *additional, void *node)
{
    free(node);
}

int main(int argc, char *argv[])
{
    int my_result = 0;
    threadpool_t *pool;

    pool = threadpool_create(THREAD, QUEUE, 0);
    fprintf(stderr, "Pool started with %d threads and "
            "queue size of %d\n", THREAD, QUEUE);

    //given datas
    long *data = malloc(DATASIZE * sizeof(long));
    for (int i = 0; i < DATASIZE; i++)
        data[i] = i + 1;
    //data end

    threadpool_reduce_t reduce_spec = {
        .begin = data,
        .end = data + DATASIZE,
        .object_size = sizeof(*data),
        .result = &my_result,
        .additional = NULL,
        .reduce = my_reduce,
        .reduce_alloc_neutral = my_alloc_neutral,
        .reduce_finish = my_merge,
        .reduce_free = my_free,
    };
    
    //setup and check , if not valid then shutdown whole program
    if(threadpool_mapreduce_setup(pool , &reduce_spec))
    {
        free(data);
        threadpool_destroy(pool , 0);
        return -1;  
    }
    
#ifdef PROFILE
    START_SW(map_time);
#endif

    threadpool_map(pool, DATASIZE, is_simple, data, 0);

#ifdef PROFILE
    STOP_SW(map_time);
#endif

#ifdef PROFILE
    START_SW(reduce_time);
#endif

    threadpool_reduce(pool);

#ifdef PROFILE
    STOP_SW(reduce_time);

    fprintf(stderr, "[map] Total time: %lf\n", GET_SEC(map_time));
    fprintf(stderr, "[is_simple] Total time: %lf\n", GET_SEC(simple_time));
    fprintf(stderr, "[reduce] Total time: %lf\n", GET_SEC(reduce_time));
#endif

    fprintf(stdout , "reduce result = %d\n", my_result);

    assert(threadpool_destroy(pool , 0) == 0);
    free(data);

    return 0;
}
