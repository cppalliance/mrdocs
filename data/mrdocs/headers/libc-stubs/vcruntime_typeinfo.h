//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_SHARE_HEADERS_LIBC_STUBS_VCRUNTIME_TYPEINFO_H
#define MRDOCS_SHARE_HEADERS_LIBC_STUBS_VCRUNTIME_TYPEINFO_H

namespace std {
class type_info {
public:
    virtual ~type_info();
    bool operator==(const type_info& rhs) const;
    bool operator!=(const type_info& rhs) const;
    bool before(const type_info& rhs) const;
    const char* name() const;
    size_t hash_code() const;
    type_info(const type_info& rhs) = delete;
    type_info& operator=(const type_info& rhs) = delete;
};
} // namespace std

#endif