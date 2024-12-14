//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_FLOAT_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_FLOAT_H

// Limits of floating-point types
#define FLT_RADIX 2
#define DECIMAL_DIG 21
#define FLT_DECIMAL_DIG 9
#define DBL_DECIMAL_DIG 17
#define LDBL_DECIMAL_DIG 21

#define FLT_MIN 1.17549435e-38F
#define DBL_MIN 2.2250738585072014e-308
#define LDBL_MIN 3.36210314311209350626e-4932L

#define FLT_TRUE_MIN 1.40129846e-45F
#define DBL_TRUE_MIN 4.9406564584124654e-324
#define LDBL_TRUE_MIN 3.64519953188247460253e-4951L

#define FLT_MAX 3.40282347e+38F
#define DBL_MAX 1.7976931348623157e+308
#define LDBL_MAX 1.18973149535723176502e+4932L

#define FLT_EPSILON 1.19209290e-7F
#define DBL_EPSILON 2.2204460492503131e-16
#define LDBL_EPSILON 1.08420217248550443401e-19L

#define FLT_DIG 6
#define DBL_DIG 15
#define LDBL_DIG 18

#define FLT_MANT_DIG 24
#define DBL_MANT_DIG 53
#define LDBL_MANT_DIG 64

#define FLT_MIN_EXP (-125)
#define DBL_MIN_EXP (-1021)
#define LDBL_MIN_EXP (-16381)

#define FLT_MIN_10_EXP (-37)
#define DBL_MIN_10_EXP (-307)
#define LDBL_MIN_10_EXP (-4931)

#define FLT_MAX_EXP 128
#define DBL_MAX_EXP 1024
#define LDBL_MAX_EXP 16384

#define FLT_MAX_10_EXP 38
#define DBL_MAX_10_EXP 308
#define LDBL_MAX_10_EXP 4932

#define FLT_ROUNDS 1
#define FLT_EVAL_METHOD 0

#define FLT_HAS_SUBNORM 1
#define DBL_HAS_SUBNORM 1
#define LDBL_HAS_SUBNORM 1

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_FLOAT_H
