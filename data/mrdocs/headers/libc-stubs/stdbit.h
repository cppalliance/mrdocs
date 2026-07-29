//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDBIT_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDBIT_H

// Function declarations for bit manipulation
int popcount(unsigned int x);
int popcountl(unsigned long x);
int popcountll(unsigned long long x);

int clz(unsigned int x);
int clzl(unsigned long x);
int clzll(unsigned long long x);

int ctz(unsigned int x);
int ctzl(unsigned long x);
int ctzll(unsigned long long x);

int ffs(unsigned int x);
int ffsl(unsigned long x);
int ffsll(unsigned long long x);

unsigned int rotr(unsigned int x, int n);
unsigned long rotrl(unsigned long x, int n);
unsigned long long rotrll(unsigned long long x, int n);

unsigned int rotl(unsigned int x, int n);
unsigned long rotll(unsigned long x, int n);
unsigned long long rotlll(unsigned long long x, int n);

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STDBIT_H
