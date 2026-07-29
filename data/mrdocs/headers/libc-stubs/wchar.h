//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_WCHAR_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_WCHAR_H

#include "stddef.h"
#include "stdint.h"

// Conversions to numeric formats
long wcstol(const wchar_t *str, wchar_t **endptr, int base);
long long wcstoll(const wchar_t *str, wchar_t **endptr, int base);
unsigned long wcstoul(const wchar_t *str, wchar_t **endptr, int base);
unsigned long long wcstoull(const wchar_t *str, wchar_t **endptr, int base);
float wcstof(const wchar_t *str, wchar_t **endptr);
double wcstod(const wchar_t *str, wchar_t **endptr);
long double wcstold(const wchar_t *str, wchar_t **endptr);
intmax_t wcstoimax(const wchar_t *str, wchar_t **endptr, int base);
uintmax_t wcstoumax(const wchar_t *str, wchar_t **endptr, int base);

// String manipulation
wchar_t *wcscpy(wchar_t *dest, const wchar_t *src);
errno_t wcscpy_s(wchar_t *dest, rsize_t destsz, const wchar_t *src);
wchar_t *wcsncpy(wchar_t *dest, const wchar_t *src, size_t n);
errno_t wcsncpy_s(wchar_t *dest, rsize_t destsz, const wchar_t *src, rsize_t count);
wchar_t *wcscat(wchar_t *dest, const wchar_t *src);
errno_t wcscat_s(wchar_t *dest, rsize_t destsz, const wchar_t *src);
wchar_t *wcsncat(wchar_t *dest, const wchar_t *src, size_t n);
errno_t wcsncat_s(wchar_t *dest, rsize_t destsz, const wchar_t *src, rsize_t count);
size_t wcsxfrm(wchar_t *dest, const wchar_t *src, size_t n);

// String examination
size_t wcslen(const wchar_t *str);
size_t wcsnlen_s(const wchar_t *str, size_t maxsize);
int wcscmp(const wchar_t *str1, const wchar_t *str2);
int wcsncmp(const wchar_t *str1, const wchar_t *str2, size_t n);
int wcscoll(const wchar_t *str1, const wchar_t *str2);
wchar_t *wcschr(const wchar_t *str, wchar_t ch);
wchar_t *wcsrchr(const wchar_t *str, wchar_t ch);
size_t wcsspn(const wchar_t *str1, const wchar_t *str2);
size_t wcscspn(const wchar_t *str1, const wchar_t *str2);
wchar_t *wcspbrk(const wchar_t *str1, const wchar_t *str2);
wchar_t *wcsstr(const wchar_t *haystack, const wchar_t *needle);
wchar_t *wcstok(wchar_t *str, const wchar_t *delim, wchar_t **saveptr);
errno_t wcstok_s(wchar_t *str, rsize_t *strmax, const wchar_t *delim, wchar_t **context);

// Wide character array manipulation
wchar_t *wmemcpy(wchar_t *dest, const wchar_t *src, size_t n);
errno_t wmemcpy_s(wchar_t *dest, rsize_t destsz, const wchar_t *src, rsize_t count);
wchar_t *wmemmove(wchar_t *dest, const wchar_t *src, size_t n);
errno_t wmemmove_s(wchar_t *dest, rsize_t destsz, const wchar_t *src, rsize_t count);
int wmemcmp(const wchar_t *str1, const wchar_t *str2, size_t n);
wchar_t *wmemchr(const wchar_t *str, wchar_t ch, size_t n);
wchar_t *wmemset(wchar_t *str, wchar_t ch, size_t n);

// Macros
#define WEOF ((wint_t)-1)

struct mbstate_t {
    int __state;
};

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_WCHAR_H
