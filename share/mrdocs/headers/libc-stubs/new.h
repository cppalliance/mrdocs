//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_NEW_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_NEW_H

#if defined(_LIBCPP_ABI_VCRUNTIME)
namespace std {
    enum class align_val_t : size_t {};
    struct nothrow_t { explicit nothrow_t() = default; };
    extern const std::nothrow_t nothrow;
}

// Replaceable allocation functions
void* operator new  ( std::size_t count );
void* operator new[]( std::size_t count );
void* operator new  ( std::size_t count, std::align_val_t al );
void* operator new[]( std::size_t count, std::align_val_t al );

// Replaceable non-throwing allocation functions
void* operator new  ( std::size_t count, const std::nothrow_t& tag );
void* operator new[]( std::size_t count, const std::nothrow_t& tag );
void* operator new  ( std::size_t count, std::align_val_t al,
                      const std::nothrow_t& tag ) noexcept;
void* operator new[]( std::size_t count, std::align_val_t al,
                      const std::nothrow_t& tag ) noexcept;

// Non-allocating placement allocation functions
void* operator new  ( std::size_t count, void* ptr );
void* operator new[]( std::size_t count, void* ptr );
#endif

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_NEW_H
