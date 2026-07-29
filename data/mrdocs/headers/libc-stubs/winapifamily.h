//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_WINAPIFAMILY_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_WINAPIFAMILY_H

#include "winpackagefamily.h"

#define WINAPI_FAMILY_PC_APP               2
#define WINAPI_FAMILY_PHONE_APP            3
#define WINAPI_FAMILY_SYSTEM               4
#define WINAPI_FAMILY_SERVER               5
#define WINAPI_FAMILY_DESKTOP_APP          100
#define WINAPI_FAMILY_APP  WINAPI_FAMILY_PC_APP
#ifndef WINAPI_FAMILY
#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP
#endif

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_WINAPIFAMILY_H
