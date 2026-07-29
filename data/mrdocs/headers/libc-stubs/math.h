//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_MATH_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_MATH_H

#ifdef _LIBCPP_MSVCRT
#include <float.h>

// Basic operations
float       fabsf( float arg );
double      fabs( double arg );
long double fabsl( long double arg );

float       fmodf( float x, float y );
double      fmod( double x, double y );
long double fmodl( long double x, long double y );

float       remainderf( float x, float y );
double      remainder( double x, double y );
long double remainderl( long double x, long double y );

float       remquof( float x, float y, int *quo );
double      remquo( double x, double y, int *quo );
long double remquol( long double x, long double y, int *quo );

float       fmaf( float x, float y, float z );
double      fma( double x, double y, double z );
long double fmal( long double x, long double y, long double z );

float       fmaxf( float x, float y );
double      fmax( double x, double y );
long double fmaxl( long double x, long double y );

float       fminf( float x, float y );
double      fmin( double x, double y );
long double fminl( long double x, long double y );

float       fdimf( float x, float y );
double      fdim( double x, double y );
long double fdiml( long double x, long double y );

float       nanf( const char* arg );
double      nan( const char* arg );
long double nanl( const char* arg );

// Exponential functions
float       expf( float arg );
double      exp( double arg );
long double expl( long double arg );

float       exp2f( float n );
double      exp2( double n );
long double exp2l( long double n );

float       expm1f( float arg );
double      expm1( double arg );
long double expm1l( long double arg );

float       logf( float arg );
double      log( double arg );
long double logl( long double arg );

float       log10f( float arg );
double      log10( double arg );
long double log10l( long double arg );

float       log2f( float arg );
double      log2( double arg );
long double log2l( long double arg );

float       log1pf( float arg );
double      log1p( double arg );
long double log1pl( long double arg );

// Power functions
float powf( float base, float exponent );
double pow( double base, double exponent );
long double powl( long double base, long double exponent );

float       sqrtf( float arg );
double      sqrt( double arg );
long double sqrtl( long double arg );

float       cbrtf( float arg );
double      cbrt( double arg );
long double cbrtl( long double arg );

float       hypotf( float x, float y );
double      hypot( double x, double y );
long double hypotl( long double x, long double y );

// Trigonometric functions
float       sinf( float arg );
double      sin( double arg );
long double sinl( long double arg );

float       cosf( float arg );
double      cos( double arg );
long double cosl( long double arg );

float       tanf( float arg );
double      tan( double arg );
long double tanl( long double arg );

float       asinf( float arg );
double      asin( double arg );
long double asinl( long double arg );

float       acosf( float arg );
double      acos( double arg );
long double acosl( long double arg );

float       atanf( float arg );
double      atan( double arg );
long double atanl( long double arg );

float       atan2f( float y, float x );
double      atan2( double y, double x );
long double atan2l( long double y, long double x );

// Hyperbolic functions
float       sinhf( float arg );
double      sinh( double arg );
long double sinhl( long double arg );

float       coshf( float arg );
double      cosh( double arg );
long double coshl( long double arg );

float       tanhf( float arg );
double      tanh( double arg );
long double tanhl( long double arg );

float       asinhf( float arg );
double      asinh( double arg );
long double asinhl( long double arg );

float       acoshf( float arg );
double      acosh( double arg );
long double acoshl( long double arg );

float       atanhf( float arg );
double      atanh( double arg );
long double atanhl( long double arg );

// Error and gamma functions
float       erff( float arg );
double      erf( double arg );
long double erfl( long double arg );

float       erfcf( float arg );
double      erfc( double arg );
long double erfcl( long double arg );

float       tgammaf( float arg );
double      tgamma( double arg );
long double tgammal( long double arg );

float       lgammaf( float arg );
double      lgamma( double arg );
long double lgammal( long double arg );

// Nearest integer floating-point operations
float       ceilf( float arg );
double      ceil( double arg );
long double ceill( long double arg );

float       floorf( float arg );
double      floor( double arg );
long double floorl( long double arg );

float       truncf( float arg );
double      trunc( double arg );
long double truncl( long double arg );

float       roundf( float arg );
double      round( double arg );
long double roundl( long double arg );

long        lroundf( float arg );
long        lround( double arg );
long        lroundl( long double arg );

long long   llroundf( float arg );
long long   llround( double arg );
long long   llroundl( long double arg );

float       nearbyintf( float arg );
double      nearbyint( double arg );
long double nearbyintl( long double arg );

float       rintf( float arg );
double      rint( double arg );
long double rintl( long double arg );

long        lrintf( float arg );
long        lrint( double arg );
long        lrintl( long double arg );

long long   llrintf( float arg );
long long   llrint( double arg );
long long   llrintl( long double arg );

// Floating-point manipulation functions
float       frexpf( float arg, int* exp );
double      frexp( double arg, int* exp );
long double frexpl( long double arg, int* exp );

float       ldexpf( float arg, int exp );
double      ldexp( double arg, int exp );
long double ldexpl( long double arg, int exp );

float       modff( float arg, float* iptr );
double      modf( double arg, double* iptr );
long double modfl( long double arg, long double* iptr );

float       scalbnf( float arg, int exp );
double      scalbn( double arg, int exp );
long double scalbnl( long double arg, int exp );

float       scalblnf( float arg, long exp );
double      scalbln( double arg, long exp );
long double scalblnl( long double arg, long exp );

int        ilogbf( float arg );
int        ilogb( double arg );
int        ilogbl( long double arg );

float       logbf( float arg );
double      logb( double arg );
long double logbl( long double arg );

float       nextafterf( float x, float y );
double      nextafter( double x, double y );
long double nextafterl( long double x, long double y );

float       nexttowardf( float x, long double y );
double      nexttoward( double x, long double y );
long double nexttowardl( long double x, long double y );

float       copysignf( float x, float y );
double      copysign( double x, double y );
long double copysignl( long double x, long double y );

int fpclassify( float num );
int fpclassify( double num );
int fpclassify( long double num );

bool isfinite( float num );
bool isfinite( double num );
bool isfinite( long double num );

bool isinf( float num );
bool isinf( double num );
bool isinf( long double num );

bool isnan( float num );
bool isnan( double num );
bool isnan( long double num );

bool isnormal( float num );
bool isnormal( double num );
bool isnormal( long double num );

bool signbit( float num );
bool signbit( double num );
bool signbit( long double num );

bool isgreater( float x, float y );
bool isgreater( double x, double y );
bool isgreater( long double x, long double y );

bool isgreaterequal( float x, float y );
bool isgreaterequal( double x, double y );
bool isgreaterequal( long double x, long double y );

bool isless( float x, float y );
bool isless( double x, double y );
bool isless( long double x, long double y );

bool islessequal( float x, float y );
bool islessequal( double x, double y );
bool islessequal( long double x, long double y );

bool islessgreater( float x, float y );
bool islessgreater( double x, double y );
bool islessgreater( long double x, long double y );

bool isunordered( float x, float y );
bool isunordered( double x, double y );
bool isunordered( long double x, long double y );
#endif // _LIBCPP_MSVCRT

// Additional macros
#ifndef HUGE_VALF
#define HUGE_VALF  __builtin_huge_valf()
#endif

#ifndef HUGE_VAL
#define HUGE_VAL   __builtin_huge_val()
#endif

#ifndef HUGE_VALL
#define HUGE_VALL  __builtin_huge_vall()
#endif

#ifndef INFINITY
#define INFINITY   __builtin_inff()
#endif

#ifndef NAN
#define NAN        __builtin_nanf("")
#endif


#ifndef FP_FAST_FMAF
#define FP_FAST_FMAF  1
#endif

#ifndef FP_FAST_FMA
#define FP_FAST_FMA   1
#endif

#ifndef FP_FAST_FMAL
#define FP_FAST_FMAL  1
#endif


#ifndef FP_ILOGB0
#define FP_ILOGB0    (-2147483647 - 1)
#endif

#ifndef FP_ILOGBNAN
#define FP_ILOGBNAN  (-2147483647 - 1)
#endif


#ifndef MATH_ERRNO
#define MATH_ERRNO        1
#endif

#ifndef MATH_ERREXCEPT
#define MATH_ERREXCEPT    2
#endif

#ifndef math_errhandling
#define math_errhandling  (MATH_ERRNO | MATH_ERREXCEPT)
#endif

// Classification and comparison
#ifndef FP_NAN
#define FP_NAN        0
#endif

#ifndef FP_INFINITE
#define FP_INFINITE   1
#endif

#ifndef FP_ZERO
#define FP_ZERO       2
#endif

#ifndef FP_SUBNORMAL
#define FP_SUBNORMAL  3
#endif

#ifndef FP_NORMAL
#define FP_NORMAL     4
#endif



#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_MATH_H
