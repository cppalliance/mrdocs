//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDINT_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDINT_H

#include "stddef.h"

using uintmax_t = unsigned long long;
using uintptr_t = unsigned long long;
using int8_t = signed char;
using int16_t = short;
using int32_t = int;
using int64_t = long long;
using int_fast8_t = signed char;
using int_fast16_t = int;
using int_fast32_t = int;
using int_fast64_t = long long;
using int_least8_t = signed char;
using int_least16_t = short;
using int_least32_t = int;
using int_least64_t = long long;
using intmax_t = long long;
using intptr_t = long long;
using uint8_t = unsigned char;
using uint16_t = unsigned short;
using uint32_t = unsigned int;
using uint64_t = unsigned long long;
using uint_fast8_t = unsigned char;
using uint_fast16_t = unsigned int;
using uint_fast32_t = unsigned int;
using uint_fast64_t = unsigned long long;
using uint_least8_t = unsigned char;
using uint_least16_t = unsigned short;
using uint_least32_t = unsigned int;
using uint_least64_t = unsigned long long;
using uintmax_t = unsigned long long;
using uintptr_t = unsigned long long;

#define INT8_WIDTH 8
#define INT16_WIDTH 16
#define INT32_WIDTH 32
#define INT64_WIDTH 64
#define INT_FAST8_WIDTH 8
#define INT_FAST16_WIDTH 32
#define INT_FAST32_WIDTH 32
#define INT_FAST64_WIDTH 64
#define INT_LEAST8_WIDTH 8
#define INT_LEAST16_WIDTH 16
#define INT_LEAST32_WIDTH 32
#define INT_LEAST64_WIDTH 64
#define INTMAX_WIDTH 64
#define INTPTR_WIDTH 64
#define INT8_MIN -128
#define INT16_MIN -32768
#define INT32_MIN -2147483648
#define INT64_MIN -9223372036854775808
#define INT_FAST8_MIN -128
#define INT_FAST16_MIN -2147483648
#define INT_FAST32_MIN -2147483648
#define INT_FAST64_MIN -9223372036854775808
#define INT_LEAST8_MIN -128
#define INT_LEAST16_MIN -32768
#define INT_LEAST32_MIN -2147483648
#define INT_LEAST64_MIN -9223372036854775808
#define INTMAX_MIN -9223372036854775808
#define INTPTR_MIN -9223372036854775808
#define INT8_MAX 127
#define INT16_MAX 32767
#define INT32_MAX 2147483647
#define INT64_MAX 9223372036854775807
#define INT_FAST8_MAX 127
#define INT_FAST16_MAX 2147483647
#define INT_FAST32_MAX 2147483647
#define INT_FAST64_MAX 9223372036854775807
#define INT_LEAST8_MAX 127
#define INT_LEAST16_MAX 32767
#define INT_LEAST32_MAX 2147483647
#define INT_LEAST64_MAX 9223372036854775807
#define INTMAX_MAX 9223372036854775807
#define INTPTR_MAX 9223372036854775807

#define UINT8_WIDTH 8
#define UINT16_WIDTH 16
#define UINT32_WIDTH 32
#define UINT64_WIDTH 64
#define UINT_FAST8_WIDTH 8
#define UINT_FAST16_WIDTH 32
#define UINT_FAST32_WIDTH 32
#define UINT_FAST64_WIDTH 64
#define UINT_LEAST8_WIDTH 8
#define UINT_LEAST16_WIDTH 16
#define UINT_LEAST32_WIDTH 32
#define UINT_LEAST64_WIDTH 64
#define UINTMAX_WIDTH 64
#define UINTPTR_WIDTH 64
#define UINT8_MAX 255
#define UINT16_MAX 65535
#define UINT32_MAX 4294967295
#define UINT64_MAX 18446744073709551615
#define UINT_FAST8_MAX 255
#define UINT_FAST16_MAX 4294967295
#define UINT_FAST32_MAX 4294967295
#define UINT_FAST64_MAX 18446744073709551615
#define UINT_LEAST8_MAX 255
#define UINT_LEAST16_MAX 65535
#define UINT_LEAST32_MAX 4294967295
#define UINT_LEAST64_MAX 18446744073709551615
#define UINTMAX_MAX 18446744073709551615
#define UINTPTR_MAX 18446744073709551615

#define PTRDIFF_WIDTH 64
#define PTRDIFF_MIN -9223372036854775808
#define PTRDIFF_MAX 9223372036854775807
#define SIZE_WIDTH 64
#define SIZE_MAX 18446744073709551615
#define SIG_ATOMIC_WIDTH 32
#define SIG_ATOMIC_MIN -2147483648
#define SIG_ATOMIC_MAX 2147483647
#define WINT_WIDTH 32
#define WINT_MIN -2147483648
#define WINT_MAX 2147483647
#define WCHAR_WIDTH 32
#define WCHAR_MIN -2147483648
#define WCHAR_MAX 2147483647

#define INT8_C(value) value
#define INT16_C(value) value
#define INT32_C(value) value
#define INT64_C(value) value##LL
#define INTMAX_C(value) value##LL
#define UINT8_C(value) value
#define UINT16_C(value) value
#define UINT32_C(value) value
#define UINT64_C(value) value##ULL
#define UINTMAX_C(value) value##ULL

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDINT_H
