//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDBOOL_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDBOOL_H

// Convenience macros for boolean values
#define bool _Bool
#define true 1
#define false 0

// Macro constant indicating the presence of boolean features
#define __bool_true_false_are_defined 1

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDBOOL_H
