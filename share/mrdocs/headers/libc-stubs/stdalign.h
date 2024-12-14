//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDALIGN_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDALIGN_H

// Convenience macros for alignment
#define alignas _Alignas
#define alignof _Alignof

// Macro constants indicating the presence of alignment features
#define __alignas_is_defined 1
#define __alignof_is_defined 1

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDALIGN_H
