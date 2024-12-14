//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDLIB_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDLIB_H

#include "stdint.h"
#include "stddef.h"
#include "malloc.h"

struct div_t { int quot; int rem; };
struct ldiv_t { long quot; long rem; };
struct lldiv_t { long long quot; long long rem; };
struct imaxdiv_t { intmax_t quot; intmax_t rem; };

#define MB_CUR_MAX 1

#define RSIZE_MAX (SIZE_MAX >> 1)

// Program termination
void abort(void);
void exit(int status);
void quick_exit(int status);
void _Exit(int status);
int atexit(void (*func)(void));
int at_quick_exit(void (*func)(void));

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

// Communicating with the environment
int system(const char *command);
char* getenv(const char *name);
errno_t getenv_s(size_t *len, char *value, rsize_t maxsize, const char *name);

// Memory alignment query
size_t memalignment(const void *ptr);

typedef void (*constraint_handler_t)(const char* __restrict msg, void* __restrict ptr, errno_t error);
constraint_handler_t set_constraint_handler_s(constraint_handler_t handler);

void abort_handler_s( const char * __restrict msg, void * __restrict ptr, errno_t error );

void ignore_handler_s( const char * __restrict msg, void * __restrict ptr, errno_t error );

void *malloc( size_t size );

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDLIB_H
