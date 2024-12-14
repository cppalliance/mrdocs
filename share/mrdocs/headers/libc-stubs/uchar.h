//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_UCHAR_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_UCHAR_H

#include "stddef.h"
#include "wchar.h"

// Types
#ifndef __cpp_char8_t
typedef unsigned char char8_t;
#endif

#ifndef __cpp_unicode_characters
typedef unsigned short char16_t;
typedef unsigned int char32_t;
#endif

// Functions
size_t mbrtoc8(char8_t *pc8, const char *s, size_t n, mbstate_t *ps);
size_t c8rtomb(char *s, char8_t c8, mbstate_t *ps);
size_t mbrtoc16(char16_t *pc16, const char *s, size_t n, mbstate_t *ps);
size_t c16rtomb(char *s, char16_t c16, mbstate_t *ps);
size_t mbrtoc32(char32_t *pc32, const char *s, size_t n, mbstate_t *ps);
size_t c32rtomb(char *s, char32_t c32, mbstate_t *ps);

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_UCHAR_H
