//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_DETAIL_FORMATTER_HPP
#define MRDOX_DETAIL_FORMATTER_HPP

#include <llvm/Support/Error.h>
#include <source_location>
#include <system_error>

namespace clang {
namespace mrdox {
namespace detail {

template<class T>
T& nice(T& t)
{
    return t;
}

template<class T>
T&& nice(T&& t)
{
    return std::forward<T>(t);
}

template<class T>
auto nice(llvm::Expected<T>&& e)
{
    return nice(e.takeError());
}

inline auto nice(std::error_code ec)
{
    return ec.message();
}

template<class T>
auto nice(llvm::ErrorOr<T>&& e)
{
    return nice(e.getError());
}

llvm::StringRef nice(std::source_location loc);

} // detail
} // mrdox
} // clang

#endif
