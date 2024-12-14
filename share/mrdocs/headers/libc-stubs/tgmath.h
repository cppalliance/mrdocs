//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_TGMATH_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_TGMATH_H

#include "math.h"
#include "complex.h"

// Type-generic macros for complex/real functions
#define fabs(x) _Generic((x), \
    float: fabsf, \
    double: fabs, \
    long double: fabsl, \
    float _Complex: cabsf, \
    double _Complex: cabs, \
    long double _Complex: cabsl)(x)

#define exp(x) _Generic((x), \
    float: expf, \
    double: exp, \
    long double: expl, \
    float _Complex: cexpf, \
    double _Complex: cexp, \
    long double _Complex: cexpl)(x)

#define log(x) _Generic((x), \
    float: logf, \
    double: log, \
    long double: logl, \
    float _Complex: clogf, \
    double _Complex: clog, \
    long double _Complex: clogl)(x)

#define pow(x, y) _Generic((x), \
    float: powf, \
    double: pow, \
    long double: powl, \
    float _Complex: cpowf, \
    double _Complex: cpow, \
    long double _Complex: cpowl)(x, y)

#define sqrt(x) _Generic((x), \
    float: sqrtf, \
    double: sqrt, \
    long double: sqrtl, \
    float _Complex: csqrtf, \
    double _Complex: csqrt, \
    long double _Complex: csqrtl)(x)

#define sin(x) _Generic((x), \
    float: sinf, \
    double: sin, \
    long double: sinl, \
    float _Complex: csinf, \
    double _Complex: csin, \
    long double _Complex: csinl)(x)

#define cos(x) _Generic((x), \
    float: cosf, \
    double: cos, \
    long double: cosl, \
    float _Complex: ccosf, \
    double _Complex: ccos, \
    long double _Complex: ccosl)(x)

#define tan(x) _Generic((x), \
    float: tanf, \
    double: tan, \
    long double: tanl, \
    float _Complex: ctanf, \
    double _Complex: ctan, \
    long double _Complex: ctanl)(x)

#define asin(x) _Generic((x), \
    float: asinf, \
    double: asin, \
    long double: asinl, \
    float _Complex: casinf, \
    double _Complex: casin, \
    long double _Complex: casinl)(x)

#define acos(x) _Generic((x), \
    float: acosf, \
    double: acos, \
    long double: acosl, \
    float _Complex: cacosf, \
    double _Complex: cacos, \
    long double _Complex: cacosl)(x)

#define atan(x) _Generic((x), \
    float: atanf, \
    double: atan, \
    long double: atanl, \
    float _Complex: catanf, \
    double _Complex: catan, \
    long double _Complex: catanl)(x)

#define sinh(x) _Generic((x), \
    float: sinhf, \
    double: sinh, \
    long double: sinhl, \
    float _Complex: csinhf, \
    double _Complex: csinh, \
    long double _Complex: csinhl)(x)

#define cosh(x) _Generic((x), \
    float: coshf, \
    double: cosh, \
    long double: coshl, \
    float _Complex: ccoshf, \
    double _Complex: ccosh, \
    long double _Complex: ccoshl)(x)

#define tanh(x) _Generic((x), \
    float: tanhf, \
    double: tanh, \
    long double: tanhl, \
    float _Complex: ctanhf, \
    double _Complex: ctanh, \
    long double _Complex: ctanhl)(x)

#define asinh(x) _Generic((x), \
    float: asinhf, \
    double: asinh, \
    long double: asinhl, \
    float _Complex: casinhf, \
    double _Complex: casinh, \
    long double _Complex: casinhl)(x)

#define acosh(x) _Generic((x), \
    float: acoshf, \
    double: acosh, \
    long double: acoshl, \
    float _Complex: cacoshf, \
    double _Complex: cacosh, \
    long double _Complex: cacoshl)(x)

#define atanh(x) _Generic((x), \
    float: atanhf, \
    double: atanh, \
    long double: atanhl, \
    float _Complex: catanhf, \
    double _Complex: catanh, \
    long double _Complex: catanhl)(x)

// Type-generic macros for real-only functions
#define atan2(x, y) _Generic((x), \
    float: atan2f, \
    double: atan2, \
    long double: atan2l)(x, y)

#define cbrt(x) _Generic((x), \
    float: cbrtf, \
    double: cbrt, \
    long double: cbrtl)(x)

#define ceil(x) _Generic((x), \
    float: ceilf, \
    double: ceil, \
    long double: ceill)(x)

#define copysign(x, y) _Generic((x), \
    float: copysignf, \
    double: copysign, \
    long double: copysignl)(x, y)

#define erf(x) _Generic((x), \
    float: erff, \
    double: erf, \
    long double: erfl)(x)

#define erfc(x) _Generic((x), \
    float: erfcf, \
    double: erfc, \
    long double: erfcl)(x)

#define exp2(x) _Generic((x), \
    float: exp2f, \
    double: exp2, \
    long double: exp2l)(x)

#define expm1(x) _Generic((x), \
    float: expm1f, \
    double: expm1, \
    long double: expm1l)(x)

#define fdim(x, y) _Generic((x), \
    float: fdimf, \
    double: fdim, \
    long double: fdiml)(x, y)

#define floor(x) _Generic((x), \
    float: floorf, \
    double: floor, \
    long double: floorl)(x)

#define fma(x, y, z) _Generic((x), \
    float: fmaf, \
    double: fma, \
    long double: fmal)(x, y, z)

#define fmax(x, y) _Generic((x), \
    float: fmaxf, \
    double: fmax, \
    long double: fmaxl)(x, y)

#define fmin(x, y) _Generic((x), \
    float: fminf, \
    double: fmin, \
    long double: fminl)(x, y)

#define fmod(x, y) _Generic((x), \
    float: fmodf, \
    double: fmod, \
    long double: fmodl)(x, y)

#define frexp(x, exp) _Generic((x), \
    float: frexpf, \
    double: frexp, \
    long double: frexpl)(x, exp)

#define hypot(x, y) _Generic((x), \
    float: hypotf, \
    double: hypot, \
    long double: hypotl)(x, y)

#define ilogb(x) _Generic((x), \
    float: ilogbf, \
    double: ilogb, \
    long double: ilogbl)(x)

#define ldexp(x, exp) _Generic((x), \
    float: ldexpf, \
    double: ldexp, \
    long double: ldexpl)(x, exp)

#define lgamma(x) _Generic((x), \
    float: lgammaf, \
    double: lgamma, \
    long double: lgammal)(x)

#define llrint(x) _Generic((x), \
    float: llrintf, \
    double: llrint, \
    long double: llrintl)(x)

#define llround(x) _Generic((x), \
    float: llroundf, \
    double: llround, \
    long double: llroundl)(x)

#define log10(x) _Generic((x), \
    float: log10f, \
    double: log10, \
    long double: log10l)(x)

#define log1p(x) _Generic((x), \
    float: log1pf, \
    double: log1p, \
    long double: log1pl)(x)

#define log2(x) _Generic((x), \
    float: log2f, \
    double: log2, \
    long double: log2l)(x)

#define logb(x) _Generic((x), \
    float: logbf, \
    double: logb, \
    long double: logbl)(x)

#define lrint(x) _Generic((x), \
    float: lrintf, \
    double: lrint, \
    long double: lrintl)(x)

#define lround(x) _Generic((x), \
    float: lroundf, \
    double: lround, \
    long double: lroundl)(x)

#define nearbyint(x) _Generic((x), \
    float: nearbyintf, \
    double: nearbyint, \
    long double: nearbyintl)(x)

#define nextafter(x, y) _Generic((x), \
    float: nextafterf, \
    double: nextafter, \
    long double: nextafterl)(x, y)

#define nexttoward(x, y) _Generic((x), \
    float: nexttowardf, \
    double: nexttoward, \
    long double: nexttowardl)(x, y)

#define remainder(x, y) _Generic((x), \
    float: remainderf, \
    double: remainder, \
    long double: remainderl)(x, y)

#define remquo(x, y, quo) _Generic((x), \
    float: remquof, \
    double: remquo, \
    long double: remquol)(x, y, quo)

#define rint(x) _Generic((x), \
    float: rintf, \
    double: rint, \
    long double: rintl)(x)

#define round(x) _Generic((x), \
    float: roundf, \
    double: round, \
    long double: roundl)(x)

#define scalbln(x, n) _Generic((x), \
    float: scalblnf, \
    double: scalbln, \
    long double: scalblnl)(x, n)

#define scalbn(x, n) _Generic((x), \
    float: scalbnf, \
    double: scalbn, \
    long double: scalbnl)(x, n)

#define tgamma(x) _Generic((x), \
    float: tgammaf, \
    double: tgamma, \
    long double: tgammal)(x)

#define trunc(x) _Generic((x), \
    float: truncf, \
    double: trunc, \
    long double: truncl)(x)

// Type-generic macros for complex-only functions
#define carg(x) _Generic((x), \
    float _Complex: cargf, \
    double _Complex: carg, \
    long double _Complex: cargl)(x)

#define conj(x) _Generic((x), \
    float _Complex: conjf, \
    double _Complex: conj, \
    long double _Complex: conjl)(x)

#define creal(x) _Generic((x), \
    float _Complex: crealf, \
    double _Complex: creal, \
    long double _Complex: creall)(x)

#define cimag(x) _Generic((x), \
    float _Complex: cimagf, \
    double _Complex: cimag, \
    long double _Complex: cimagl)(x)

#define cproj(x) _Generic((x), \
    float _Complex: cprojf, \
    double _Complex: cproj, \
    long double _Complex: cprojl)(x)

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_TGMATH_H

