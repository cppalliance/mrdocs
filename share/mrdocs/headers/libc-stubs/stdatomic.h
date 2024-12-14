//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDATOMIC_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDATOMIC_H

#include "stddef.h"
#include "stdint.h"

// Macro constants indicating lock-free atomic types
#define ATOMIC_BOOL_LOCK_FREE 1
#define ATOMIC_CHAR_LOCK_FREE 1
#define ATOMIC_CHAR16_T_LOCK_FREE 1
#define ATOMIC_CHAR32_T_LOCK_FREE 1
#define ATOMIC_WCHAR_T_LOCK_FREE 1
#define ATOMIC_SHORT_LOCK_FREE 1
#define ATOMIC_INT_LOCK_FREE 1
#define ATOMIC_LONG_LOCK_FREE 1
#define ATOMIC_LLONG_LOCK_FREE 1
#define ATOMIC_POINTER_LOCK_FREE 1

// Function declarations for atomic operations
bool atomic_is_lock_free(const volatile void *obj);

void atomic_store(volatile void *obj, int val);
void atomic_store_explicit(volatile void *obj, int val, int order);

int atomic_load(const volatile void *obj);
int atomic_load_explicit(const volatile void *obj, int order);

int atomic_exchange(volatile void *obj, int val);
int atomic_exchange_explicit(volatile void *obj, int val, int order);

bool atomic_compare_exchange_strong(volatile void *obj, int *expected, int desired);
bool atomic_compare_exchange_strong_explicit(volatile void *obj, int *expected, int desired, int success, int failure);
bool atomic_compare_exchange_weak(volatile void *obj, int *expected, int desired);
bool atomic_compare_exchange_weak_explicit(volatile void *obj, int *expected, int desired, int success, int failure);

int atomic_fetch_add(volatile void *obj, int arg);
int atomic_fetch_add_explicit(volatile void *obj, int arg, int order);

int atomic_fetch_sub(volatile void *obj, int arg);
int atomic_fetch_sub_explicit(volatile void *obj, int arg, int order);

int atomic_fetch_or(volatile void *obj, int arg);
int atomic_fetch_or_explicit(volatile void *obj, int arg, int order);

int atomic_fetch_xor(volatile void *obj, int arg);
int atomic_fetch_xor_explicit(volatile void *obj, int arg, int order);

int atomic_fetch_and(volatile void *obj, int arg);
int atomic_fetch_and_explicit(volatile void *obj, int arg, int order);

// Struct and function declarations for atomic_flag
typedef struct {
    bool _Value;
} atomic_flag;

bool atomic_flag_test_and_set(volatile atomic_flag *obj);
bool atomic_flag_test_and_set_explicit(volatile atomic_flag *obj, int order);

void atomic_flag_clear(volatile atomic_flag *obj);
void atomic_flag_clear_explicit(volatile atomic_flag *obj, int order);

// Function declarations for initialization
void atomic_init(volatile void *obj, int val);

#define ATOMIC_VAR_INIT(value) (value)
#define ATOMIC_FLAG_INIT {0}

// Enum for memory_order
typedef enum {
    memory_order_relaxed,
    memory_order_consume,
    memory_order_acquire,
    memory_order_release,
    memory_order_acq_rel,
    memory_order_seq_cst
} memory_order;

// Function declarations for memory synchronization
#define kill_dependency(y) (y)

void atomic_thread_fence(memory_order order);
void atomic_signal_fence(memory_order order);

// Convenience type aliases
typedef _Atomic bool atomic_bool;
typedef _Atomic char atomic_char;
typedef _Atomic signed char atomic_schar;
typedef _Atomic unsigned char atomic_uchar;
typedef _Atomic short atomic_short;
typedef _Atomic unsigned short atomic_ushort;
typedef _Atomic int atomic_int;
typedef _Atomic unsigned int atomic_uint;
typedef _Atomic long atomic_long;
typedef _Atomic unsigned long atomic_ulong;
typedef _Atomic long long atomic_llong;
typedef _Atomic unsigned long long atomic_ullong;
typedef _Atomic char8_t atomic_char8_t;
typedef _Atomic char16_t atomic_char16_t;
typedef _Atomic char32_t atomic_char32_t;
typedef _Atomic wchar_t atomic_wchar_t;
typedef _Atomic int_least8_t atomic_int_least8_t;
typedef _Atomic uint_least8_t atomic_uint_least8_t;
typedef _Atomic int_least16_t atomic_int_least16_t;
typedef _Atomic uint_least16_t atomic_uint_least16_t;
typedef _Atomic int_least32_t atomic_int_least32_t;
typedef _Atomic uint_least32_t atomic_uint_least32_t;
typedef _Atomic int_least64_t atomic_int_least64_t;
typedef _Atomic uint_least64_t atomic_uint_least64_t;
typedef _Atomic int_fast8_t atomic_int_fast8_t;
typedef _Atomic uint_fast8_t atomic_uint_fast8_t;
typedef _Atomic int_fast16_t atomic_int_fast16_t;
typedef _Atomic uint_fast16_t atomic_uint_fast16_t;
typedef _Atomic int_fast32_t atomic_int_fast32_t;
typedef _Atomic uint_fast32_t atomic_uint_fast32_t;
typedef _Atomic int_fast64_t atomic_int_fast64_t;
typedef _Atomic uint_fast64_t atomic_uint_fast64_t;
typedef _Atomic intptr_t atomic_intptr_t;
typedef _Atomic uintptr_t atomic_uintptr_t;
typedef _Atomic size_t atomic_size_t;
typedef _Atomic ptrdiff_t atomic_ptrdiff_t;
typedef _Atomic intmax_t atomic_intmax_t;
typedef _Atomic uintmax_t atomic_uintmax_t;

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDATOMIC_H
