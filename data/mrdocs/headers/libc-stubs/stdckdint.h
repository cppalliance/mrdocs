//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDCKDINT_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDCKDINT_H

// Macros for performing checked integer arithmetic
#define ckd_add(result, a, b) __builtin_add_overflow(a, b, result)
#define ckd_sub(result, a, b) __builtin_sub_overflow(a, b, result)
#define ckd_mul(result, a, b) __builtin_mul_overflow(a, b, result)

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDCKDINT_H
