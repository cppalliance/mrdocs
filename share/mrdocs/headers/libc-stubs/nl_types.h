//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_NL_TYPES_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_NL_TYPES_H

using nl_catd = int;
using nl_item = int;

int       catclose(nl_catd);
char     *catgets(nl_catd, int, int, const char *);
nl_catd   catopen(const char *, int);

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_NL_TYPES_H
