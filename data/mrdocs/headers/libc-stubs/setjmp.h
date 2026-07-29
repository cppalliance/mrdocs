//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_SETJMP_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_SETJMP_H

// Types
typedef struct {
    void* sp;  // Stack pointer
    void* pc;  // Program counter
    int val;   // Return value
} jmp_buf[1];

#ifdef __cplusplus
extern "C" {
#endif

// Non-local jumps
#define setjmp(env) _setjmp(env)
int __cdecl _setjmp(jmp_buf env);

#ifdef __cplusplus
}
#endif

void longjmp(jmp_buf env, int val);

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_SETJMP_H
