//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDARG_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDARG_H

// Type
typedef __builtin_va_list va_list;

// Macros
#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_copy(dest, src) __builtin_va_copy(dest, src)
#define va_end(ap) __builtin_va_end(ap)

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDARG_H
