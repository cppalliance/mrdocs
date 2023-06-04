//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_SUPPORT_ERROR_HPP
#define MRDOX_SUPPORT_ERROR_HPP

#include <mrdox/Support/Error.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>

template<>
struct fmt::formatter<llvm::StringRef>
    : fmt::formatter<std::string_view>
{
    auto format(
        llvm::StringRef s,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string_view>::format(
            std::string_view(s.data(), s.size()), ctx);
    }
};

template<unsigned InternalLen>
struct fmt::formatter<llvm::SmallString<InternalLen>>
    : fmt::formatter<std::string_view>
{
    auto format(
        llvm::SmallString<InternalLen> const& s,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string_view>::format(
            std::string_view(s.data(), s.size()), ctx);
    }
};

namespace clang {
namespace mrdox {

inline
Error
toError(llvm::Error err)
{
    return Error(toString(std::move(err)));
}

} // mrdox
} // clang

#endif
