//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_VCRUNTIME_EXCEPTION_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_VCRUNTIME_EXCEPTION_H

namespace std {
    class exception {
    public:
        exception() noexcept;
        exception( const exception& other ) noexcept;
        virtual ~exception();
        exception& operator=( const exception& other ) noexcept;
        virtual const char* what() const noexcept;
    };
}

#endif // MRDOCS_SHARE_HEADERS_LIBC_STUBS_VCRUNTIME_EXCEPTION_H
