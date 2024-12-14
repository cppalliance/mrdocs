//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_COMPLEX_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_COMPLEX_H

// Types
#define imaginary _Imaginary
#define complex _Complex

// Constants
#define _Imaginary_I ((float _Imaginary)1.0)
#define _Complex_I ((float _Complex)1.0)
#define I _Complex_I

// Manipulation
#define CMPLX(x, y) ((double _Complex)((x) + _Complex_I * (y)))
#define CMPLXF(x, y) ((float _Complex)((x) + _Complex_I * (y)))
#define CMPLXL(x, y) ((long double _Complex)((x) + _Complex_I * (y)))

double creal(double _Complex z);
float crealf(float _Complex z);
long double creall(long double _Complex z);

double cimag(double _Complex z);
float cimagf(float _Complex z);
long double cimagl(long double _Complex z);

double cabs(double _Complex z);
float cabsf(float _Complex z);
long double cabsl(long double _Complex z);

double carg(double _Complex z);
float cargf(float _Complex z);
long double cargl(long double _Complex z);

double _Complex conj(double _Complex z);
float _Complex conjf(float _Complex z);
long double _Complex conjl(long double _Complex z);

double _Complex cproj(double _Complex z);
float _Complex cprojf(float _Complex z);
long double _Complex cprojl(long double _Complex z);

// Exponential functions
double _Complex cexp(double _Complex z);
float _Complex cexpf(float _Complex z);
long double _Complex cexpl(long double _Complex z);

double _Complex clog(double _Complex z);
float _Complex clogf(float _Complex z);
long double _Complex clogl(long double _Complex z);

// Power functions
double _Complex cpow(double _Complex x, double _Complex y);
float _Complex cpowf(float _Complex x, float _Complex y);
long double _Complex cpowl(long double _Complex x, long double _Complex y);

double _Complex csqrt(double _Complex z);
float _Complex csqrtf(float _Complex z);
long double _Complex csqrtl(long double _Complex z);

// Trigonometric functions
double _Complex csin(double _Complex z);
float _Complex csinf(float _Complex z);
long double _Complex csinl(long double _Complex z);

double _Complex ccos(double _Complex z);
float _Complex ccosf(float _Complex z);
long double _Complex ccosl(long double _Complex z);

double _Complex ctan(double _Complex z);
float _Complex ctanf(float _Complex z);
long double _Complex ctanl(long double _Complex z);

double _Complex casin(double _Complex z);
float _Complex casinf(float _Complex z);
long double _Complex casinl(long double _Complex z);

double _Complex cacos(double _Complex z);
float _Complex cacosf(float _Complex z);
long double _Complex cacosl(long double _Complex z);

double _Complex catan(double _Complex z);
float _Complex catanf(float _Complex z);
long double _Complex catanl(long double _Complex z);

// Hyperbolic functions
double _Complex csinh(double _Complex z);
float _Complex csinhf(float _Complex z);
long double _Complex csinhl(long double _Complex z);

double _Complex ccosh(double _Complex z);
float _Complex ccoshf(float _Complex z);
long double _Complex ccoshl(long double _Complex z);

double _Complex ctanh(double _Complex z);
float _Complex ctanhf(float _Complex z);
long double _Complex ctanhl(long double _Complex z);

double _Complex casinh(double _Complex z);
float _Complex casinhf(float _Complex z);
long double _Complex casinhl(long double _Complex z);

double _Complex cacosh(double _Complex z);
float _Complex cacoshf(float _Complex z);
long double _Complex cacoshl(long double _Complex z);

double _Complex catanh(double _Complex z);
float _Complex catanhf(float _Complex z);
long double _Complex catanhl(long double _Complex z);

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_COMPLEX_H
