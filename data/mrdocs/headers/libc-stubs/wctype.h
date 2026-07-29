//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_WCTYPE_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_WCTYPE_H

#include "stddef.h"
#include "wchar.h"

// Types
typedef int wctrans_t;
typedef int wctype_t;

// Character classification
int iswalnum(wint_t wc);
int iswalpha(wint_t wc);
int iswlower(wint_t wc);
int iswupper(wint_t wc);
int iswdigit(wint_t wc);
int iswxdigit(wint_t wc);
int iswcntrl(wint_t wc);
int iswgraph(wint_t wc);
int iswspace(wint_t wc);
int iswblank(wint_t wc);
int iswprint(wint_t wc);
int iswpunct(wint_t wc);
int iswctype(wint_t wc, wctype_t desc);
wctype_t wctype(const char *property);

// Character manipulation
wint_t towlower(wint_t wc);
wint_t towupper(wint_t wc);
wint_t towctrans(wint_t wc, wctrans_t desc);
wctrans_t wctrans(const char *property);

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_WCTYPE_H
