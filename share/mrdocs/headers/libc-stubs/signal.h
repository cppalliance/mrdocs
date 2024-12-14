//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_SIGNAL_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_SIGNAL_H

// Types
typedef int sig_atomic_t;

// Signal handling strategies
#define SIG_DFL ((void (*)(int))0)  // Default signal handling
#define SIG_IGN ((void (*)(int))1)  // Ignore signal
#define SIG_ERR ((void (*)(int))-1) // Error return

// Signal types
#define SIGABRT 6  // Abort signal
#define SIGFPE  8  // Floating-point exception
#define SIGILL  4  // Illegal instruction
#define SIGINT  2  // Interrupt signal
#define SIGSEGV 11 // Segmentation fault
#define SIGTERM 15 // Termination signal

// Function declarations
typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler);
int raise(int sig);

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_SIGNAL_H
