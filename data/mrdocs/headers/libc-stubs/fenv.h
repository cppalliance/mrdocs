//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_FENV_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_FENV_H

// Types
typedef struct {
    unsigned int __control_word;
    unsigned int __status_word;
    unsigned int __tag_word;
    unsigned int __other_flags;
} fenv_t;

typedef unsigned int fexcept_t;

// Functions
int feclearexcept(int excepts);
int fetestexcept(int excepts);
int feraiseexcept(int excepts);
int fegetexceptflag(fexcept_t* flagp, int excepts);
int fesetexceptflag(const fexcept_t* flagp, int excepts);
int fegetround(void);
int fesetround(int round);
int fegetenv(fenv_t* envp);
int fesetenv(const fenv_t* envp);
int feholdexcept(fenv_t* envp);
int feupdateenv(const fenv_t* envp);

// Macros for floating-point exceptions
#define FE_ALL_EXCEPT    0x3F
#define FE_DIVBYZERO     0x04
#define FE_INEXACT       0x20
#define FE_INVALID       0x01
#define FE_OVERFLOW      0x08
#define FE_UNDERFLOW     0x10

// Macros for floating-point rounding direction
#define FE_DOWNWARD      0x400
#define FE_TONEAREST     0x000
#define FE_TOWARDZERO    0xC00
#define FE_UPWARD        0x800

// Default floating-point environment
extern const fenv_t* FE_DFL_ENV;

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_FENV_H
