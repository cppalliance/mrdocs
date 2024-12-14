//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_TIME_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_TIME_H

#include "stddef.h"

// Types
typedef long time_t;
typedef long clock_t;

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

// Functions
double difftime(time_t end, time_t beginning);
time_t time(time_t *timer);
clock_t clock(void);
int timespec_get(struct timespec *ts, int base);
int timespec_getres(struct timespec *ts, int base);

char *asctime(const struct tm *timeptr);
errno_t asctime_s(char *buf, rsize_t bufsz, const struct tm *timeptr);
char *ctime(const time_t *timer);
errno_t ctime_s(char *buf, rsize_t bufsz, const time_t *timer);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);
size_t wcsftime(wchar_t *wcs, size_t maxsize, const wchar_t *format, const struct tm *timeptr);

struct tm *gmtime(const time_t *timer);
struct tm *gmtime_r(const time_t *timer, struct tm *result);
errno_t gmtime_s(struct tm *result, const time_t *timer);
struct tm *localtime(const time_t *timer);
struct tm *localtime_r(const time_t *timer, struct tm *result);
errno_t localtime_s(struct tm *result, const time_t *timer);
time_t mktime(struct tm *timeptr);

// Constants
#define CLOCKS_PER_SEC 1000000

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_TIME_H
