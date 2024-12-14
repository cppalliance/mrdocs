//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_ERRNO_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_ERRNO_H

#include "stddef.h"

// Define errno as a thread-local variable
#ifdef __cplusplus
extern "C" {
#endif

// Thread-local storage for errno
extern __thread errno_t __errno;

#ifdef __cplusplus
}
#endif

// Error codes
#define EDOM 1
#define ERANGE 2
#define EILSEQ 3

#define errno (__errno)

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_ERRNO_H
