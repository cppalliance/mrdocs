//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_LOCALE_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_LOCALE_H

#include "stdarg.h"

struct lconv {
    // Non-monetary numeric formatting parameters
    char* decimal_point;
    char* thousands_sep;
    char* grouping;
    // Monetary numeric formatting parameters
    char* mon_decimal_point;
    char* mon_thousands_sep;
    char* mon_grouping;
    char* positive_sign;
    char* negative_sign;
    // Local monetary numeric formatting parameters
    char* currency_symbol;
    char frac_digits;
    char p_cs_precedes;
    char n_cs_precedes;
    char p_sep_by_space;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
    // International monetary numeric formatting parameters
    char* int_curr_symbol;
    char int_frac_digits;
    char int_p_cs_precedes;
    char int_n_cs_precedes;
    char int_p_sep_by_space;
    char int_n_sep_by_space;
    char int_p_sign_posn;
    char int_n_sign_posn;
};

#if defined(_LIBCPP_MSVCRT_LIKE)
// Windows defines _locale_t and locale_t is defined by libc++
struct _locale_t {};
#else
// Use the same struct for Linux and MacOS
#include "xlocale.h"
#endif

#define LC_CTYPE           0
#define LC_NUMERIC         1
#define LC_TIME            2
#define LC_COLLATE         3
#define LC_MONETARY        4
#define LC_MESSAGES        5
#define LC_ALL             6
#define LC_PAPER           7
#define LC_NAME            8
#define LC_ADDRESS         9
#define LC_TELEPHONE      10
#define LC_MEASUREMENT    11
#define LC_IDENTIFICATION 12

#define LC_CTYPE_MASK          (1 << LC_CTYPE)
#define LC_NUMERIC_MASK        (1 << LC_NUMERIC)
#define LC_TIME_MASK           (1 << LC_TIME)
#define LC_COLLATE_MASK        (1 << LC_COLLATE)
#define LC_MONETARY_MASK       (1 << LC_MONETARY)
#define LC_MESSAGES_MASK       (1 << LC_MESSAGES)
#define LC_PAPER_MASK          (1 << LC_PAPER)
#define LC_NAME_MASK           (1 << LC_NAME)
#define LC_ADDRESS_MASK        (1 << LC_ADDRESS)
#define LC_TELEPHONE_MASK      (1 << LC_TELEPHONE)
#define LC_MEASUREMENT_MASK    (1 << LC_MEASUREMENT)
#define LC_IDENTIFICATION_MASK (1 << LC_IDENTIFICATION)
#define LC_ALL_MASK (LC_CTYPE_MASK | LC_NUMERIC_MASK | LC_TIME_MASK | LC_COLLATE_MASK | \
LC_MONETARY_MASK | LC_MESSAGES_MASK | LC_PAPER_MASK | LC_NAME_MASK | \
LC_ADDRESS_MASK | LC_TELEPHONE_MASK | LC_MEASUREMENT_MASK | \
LC_IDENTIFICATION_MASK)

#if defined(_LIBCPP_MSVCRT_LIKE)
#define _SPACE 0x01
#define _UPPER 0x02
#define _LOWER 0x04
#define _DIGIT 0x08
#define _PUNCT 0x10
#define _CONTROL 0x20
#define _BLANK 0x40
#define _HEX 0x80
#define _LEADBYTE 0x8000
#define _ALPHA (_UPPER|_LOWER) // (0x02|0x04)
#define _ALNUM (_UPPER|_LOWER|_DIGIT) // (0x02|0x04|0x08)
#else
#define _LIBCPP_PROVIDES_DEFAULT_RUNE_TABLE
#endif

char* setlocale( int category, const char* locale );
struct lconv *localeconv(void);

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_LOCALE_H
