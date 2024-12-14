//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDNORETURN_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDNORETURN_H

// _Noreturn function specifier is deprecated. [[noreturn]] attribute should be used instead.
// The macro noreturn is also deprecated.

#define _Noreturn [[noreturn]]
#define noreturn _Noreturn

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDNORETURN_H
