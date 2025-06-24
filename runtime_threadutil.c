#ifndef M4_NO_THREAD

#include "mcp_forth.h"
#include <pthread.h>
#include <stdalign.h>
_Static_assert(alignof(pthread_mutex_t) <= 4);
_Static_assert(alignof(pthread_cond_t) <= 4);

typedef struct {
    void * buf;
    unsigned elsize;
    unsigned nels;
    unsigned next_out;
    unsigned len;
    unsigned unfinished_tasks;
    pthread_mutex_t mutex;
    pthread_cond_t not_full_cond;
    pthread_cond_t not_empty_cond;
    pthread_cond_t all_tasks_done_cond;
} queue_t;

static void queue_init(queue_t * q, void * buf, unsigned elsize, unsigned nels)
{
    int res;

    assert(nels);
    assert(elsize);

    q->buf = buf;
    q->elsize = elsize;
    q->nels = nels;
    q->next_out = 0;
    q->len = 0;
    q->unfinished_tasks = 0;
    res = pthread_mutex_init(&q->mutex, NULL);
    assert(res == 0);
    res = pthread_cond_init(&q->not_full_cond, NULL);
    assert(res == 0);
    res = pthread_cond_init(&q->not_empty_cond, NULL);
    assert(res == 0);
    res = pthread_cond_init(&q->all_tasks_done_cond, NULL);
    assert(res == 0);
}

static void queue_put(queue_t * q, const void * el)
{
    int res;

    res = pthread_mutex_lock(&q->mutex);
    assert(res == 0);

    while(q->len == q->nels) {
        res = pthread_cond_wait(&q->not_full_cond, &q->mutex);
        assert(res == 0);
    }

    unsigned next_in = q->next_out + q->len;
    if(next_in >= q->nels) next_in -= q->nels;

    void * next_in_ptr = q->buf + next_in * q->elsize;

    memcpy(next_in_ptr, el, q->elsize);

    if(q->len == 0) {
        res = pthread_cond_signal(&q->not_empty_cond);
        assert(res == 0);
    }

    q->len += 1;

    q->unfinished_tasks += 1;

    res = pthread_mutex_unlock(&q->mutex);
    assert(res == 0);
}

static void queue_get(queue_t * q, void * el)
{
    int res;

    res = pthread_mutex_lock(&q->mutex);
    assert(res == 0);

    while(q->len == 0) {
        res = pthread_cond_wait(&q->not_empty_cond, &q->mutex);
        assert(res == 0);
    }

    void * next_out_ptr = q->buf + q->next_out * q->elsize;

    memcpy(el, next_out_ptr, q->elsize);

    if(q->len == q->nels) {
        res = pthread_cond_signal(&q->not_full_cond);
        assert(res == 0);
    }

    q->len -= 1;

    q->next_out += 1;
    if(q->next_out >= q->nels) q->next_out = 0;

    res = pthread_mutex_unlock(&q->mutex);
    assert(res == 0);
}

static void queue_task_done(queue_t * q)
{
    int res;

    res = pthread_mutex_lock(&q->mutex);
    assert(res == 0);

    assert(q->unfinished_tasks);

    q->unfinished_tasks -= 1;

    if(q->unfinished_tasks == 0) {
        res = pthread_cond_signal(&q->all_tasks_done_cond);
        assert(res == 0);
    }

    res = pthread_mutex_unlock(&q->mutex);
    assert(res == 0);
}

static void queue_join(queue_t * q)
{
    int res;

    res = pthread_mutex_lock(&q->mutex);
    assert(res == 0);

    while(q->unfinished_tasks) {
        res = pthread_cond_wait(&q->all_tasks_done_cond, &q->mutex);
        assert(res == 0);
    }

    res = pthread_mutex_unlock(&q->mutex);
    assert(res == 0);
}

static void queue_destroy(queue_t * q)
{
    int res;

    res = pthread_mutex_destroy(&q->mutex);
    assert(res == 0);
    res = pthread_cond_destroy(&q->not_full_cond);
    assert(res == 0);
    res = pthread_cond_destroy(&q->not_empty_cond);
    assert(res == 0);
    res = pthread_cond_destroy(&q->all_tasks_done_cond);
    assert(res == 0);
}

const m4_runtime_cb_array_t m4_runtime_lib_threadutil[] = {
    {"queue-memsz", {m4_lit, (void *) sizeof(queue_t)}},
    {"queue-init", {m4_f04, queue_init}},
    {"queue-put", {m4_f02, queue_put}},
    {"queue-get", {m4_f02, queue_get}},
    {"queue-task-done", {m4_f01, queue_task_done}},
    {"queue-join", {m4_f01, queue_join}},
    {"queue-destroy", {m4_f01, queue_destroy}},
    {NULL},
};

#endif
