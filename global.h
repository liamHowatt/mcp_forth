#pragma once

#include "mcp_forth.h"

#ifndef M4_NO_THREAD
    typedef struct {
        bool is_main;
    } global_t;
#endif

typedef struct {
#ifndef M4_NO_THREAD
        global_t base;
#endif
    int callbacks_used;
    void * ctxs[M4_MAX_CALLBACKS];
} global_main_t;

#ifndef M4_NO_THREAD
    typedef struct {
        global_t base;
        void * ctx;
    } global_thread_t;
#endif

#ifndef M4_NO_THREAD
    extern __thread global_t * m4_global_;
#elif !defined(M4_NO_TLS)
    extern __thread global_main_t * m4_global_;
#else
    extern global_main_t m4_global_;
#endif

void * m4_global_get_ctx(int cb_i);
#ifndef M4_NO_THREAD
    void m4_global_thread_set(global_thread_t * global_thread, void * ctx);
#endif
int m4_global_main_get_callbacks_used(int * callbacks_used_dst);
void m4_global_main_callbacks_set_ctx(int callback_count, void * ctx);
