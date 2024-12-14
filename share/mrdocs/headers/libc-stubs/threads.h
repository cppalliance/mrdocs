//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_THREADS_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_THREADS_H

#include "stdint.h"
#include "stddef.h"

// Threads
typedef struct {
    // Implementation-defined
} thrd_t;

int thrd_create(thrd_t *thr, int (*func)(void *), void *arg);
int thrd_equal(thrd_t thr0, thrd_t thr1);
thrd_t thrd_current(void);
int thrd_sleep(const struct timespec *duration, struct timespec *remaining);
void thrd_yield(void);
_Noreturn void thrd_exit(int res);
int thrd_detach(thrd_t thr);
int thrd_join(thrd_t thr, int *res);

#define thrd_success 0
#define thrd_timedout 1
#define thrd_busy 2
#define thrd_nomem 3
#define thrd_error 4

typedef int (*thrd_start_t)(void *);

// Mutual exclusion
typedef struct {
    // Implementation-defined
} mtx_t;

int mtx_init(mtx_t *mtx, int type);
int mtx_lock(mtx_t *mtx);
int mtx_timedlock(mtx_t *mtx, const struct timespec *ts);
int mtx_trylock(mtx_t *mtx);
int mtx_unlock(mtx_t *mtx);
void mtx_destroy(mtx_t *mtx);

#define mtx_plain 0
#define mtx_recursive 1
#define mtx_timed 2

// Call once
typedef struct {
    // Implementation-defined
} once_flag;

#define ONCE_FLAG_INIT {0}

void call_once(once_flag *flag, void (*func)(void));

// Condition variables
typedef struct {
    // Implementation-defined
} cnd_t;

int cnd_init(cnd_t *cond);
int cnd_signal(cnd_t *cond);
int cnd_broadcast(cnd_t *cond);
int cnd_wait(cnd_t *cond, mtx_t *mtx);
int cnd_timedwait(cnd_t *cond, mtx_t *mtx, const struct timespec *ts);
void cnd_destroy(cnd_t *cond);

// Thread-local storage
#define thread_local _Thread_local

typedef void (*tss_dtor_t)(void *);
typedef struct {
    // Implementation-defined
} tss_t;

#define TSS_DTOR_ITERATIONS 4

int tss_create(tss_t *key, tss_dtor_t dtor);
void *tss_get(tss_t key);
int tss_set(tss_t key, void *val);
void tss_delete(tss_t key);

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_THREADS_H

