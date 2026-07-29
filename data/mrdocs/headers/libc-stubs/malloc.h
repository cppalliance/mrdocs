//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_MALLOC_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_MALLOC_H

void *malloc( size_t size );
void* calloc( size_t num, size_t size );
void *realloc( void *ptr, size_t new_size );
void free( void *ptr );
void free_sized( void* ptr, size_t size );
void free_aligned_sized( void* ptr, size_t alignment, size_t size );
void *aligned_alloc( size_t alignment, size_t size );

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_MALLOC_H
