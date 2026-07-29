//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_STRING_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_STRING_H

#include "stddef.h"

// String manipulation
char* strcpy(char* dest, const char* src);
errno_t strcpy_s(char* dest, rsize_t destsz, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
errno_t strncpy_s(char* dest, rsize_t destsz, const char* src, rsize_t n);
char* strcat(char* dest, const char* src);
errno_t strcat_s(char* dest, rsize_t destsz, const char* src);
char* strncat(char* dest, const char* src, size_t n);
errno_t strncat_s(char* dest, rsize_t destsz, const char* src, rsize_t n);
size_t strxfrm(char* dest, const char* src, size_t n);
char* strdup(const char* s);
char* strndup(const char* s, size_t n);

// String examination
size_t strlen(const char* s);
size_t strnlen_s(const char* s, rsize_t maxsize);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
int strcoll(const char* s1, const char* s2);
char* strchr(const char* s, int c);
char* strrchr(const char* s, int c);
size_t strspn(const char* s, const char* accept);
size_t strcspn(const char* s, const char* reject);
char* strpbrk(const char* s, const char* accept);
char* strstr(const char* haystack, const char* needle);
char* strtok(char* str, const char* delim);
errno_t strtok_s(char* str, rsize_t* strmax, const char* delim, char** context);

// Character array manipulation
void* memchr(const void* s, int c, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
void* memset(void* s, int c, size_t n);
void* memset_explicit(void* s, int c, size_t n);
errno_t memset_s(void* s, rsize_t smax, int c, rsize_t n);
void* memcpy(void* dest, const void* src, size_t n);
errno_t memcpy_s(void* dest, rsize_t destsz, const void* src, rsize_t count);
void* memmove(void* dest, const void* src, size_t n);
errno_t memmove_s(void* dest, rsize_t destsz, const void* src, rsize_t count);
void* memccpy(void* dest, const void* src, int c, size_t n);

// Miscellaneous
char* strerror(int errnum);
errno_t strerror_s(char* buf, rsize_t bufsz, int errnum);
size_t strerrorlen_s(int errnum);

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_STRING_H
