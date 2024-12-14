//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDDEF_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDDEF_H

// size_t
#ifndef _SIZE_T
#define _SIZE_T
#ifdef __SIZE_TYPE__
typedef __SIZE_TYPE__ size_t;
#else
typedef unsigned long long size_t;
#endif
#endif

// size_t
#ifndef _SIZE_T
#define _SIZE_T
#ifdef __SIZE_TYPE__
typedef __SIZE_TYPE__ rsize_t;
#else
typedef unsigned long long rsize_t;
#endif
#endif

// ptrdiff_t
#ifndef _PTRDIFF_T
#define _PTRDIFF_T
#ifdef __PTRDIFF_TYPE__
typedef __PTRDIFF_TYPE__ ptrdiff_t;
#else
typedef long long ptrdiff_t;
#endif
#endif

// rsize_t
#if defined(__STDC_WANT_LIB_EXT1__) && __STDC_WANT_LIB_EXT1__ >= 1
#define __no_need_rsize_t
#endif

#ifndef __no_need_rsize_t
typedef size_t rsize_t;
#else
#undef __no_need_rsize_t
#endif

// nullptr_t
#ifdef __cplusplus
#if defined(_MSC_EXTENSIONS) && defined(_NATIVE_NULLPTR_SUPPORTED)
namespace std {
    typedef decltype(nullptr) nullptr_t;
}
using ::std::nullptr_t;
#endif
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
typedef typeof(nullptr) nullptr_t;
#endif

// NULL
#ifdef __cplusplus
#if !defined(__MINGW32__) && !defined(_MSC_VER)
#define NULL __null
#else
#define NULL 0
#endif
#else
#define NULL ((void*)0)
#endif

// wint_t
#ifndef _WINT_T
#define _WINT_T
#ifdef __WINT_TYPE__
typedef __WINT_TYPE__ wint_t;
#else
typedef unsigned int wint_t;
#endif
#endif

// errno_t
using errno_t = int;

// max_align_t
#if defined(_MSC_VER)
typedef double max_align_t;
#elif defined(__APPLE__)
typedef long double max_align_t;
#else
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L) ||              \
    (defined(__cplusplus) && __cplusplus >= 201103L)
#define __no_need_max_align_t
#endif
#ifndef __no_need_max_align_t
// Define 'max_align_t' to match the GCC definition.
typedef struct {
    long long __clang_max_align_nonce1
        __attribute__((__aligned__(__alignof__(long long))));
    long double __clang_max_align_nonce2
        __attribute__((__aligned__(__alignof__(long double))));
} max_align_t;
#endif
#endif

// offsetof_t
#define offsetof(t, d) __builtin_offsetof(t, d)

// unreachable
#ifndef unreachable
#define unreachable() (__builtin_unreachable())
#endif

// wchar_t
#if !defined(__cplusplus) || (defined(_MSC_VER) && !_NATIVE_WCHAR_T_DEFINED)
#if !defined(_WCHAR_T) ||                                                      \
(__has_feature(modules) && !__building_module(_Builtin_stddef))
#define _WCHAR_T
#ifdef _MSC_EXTENSIONS
#define _WCHAR_T_DEFINED
#endif
typedef __WCHAR_TYPE__ wchar_t;
#endif
#endif

// tm
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

// FILE
typedef struct {
    int _file;
} FILE;

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDDEF_H
