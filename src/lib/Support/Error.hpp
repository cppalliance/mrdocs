//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_SUPPORT_ERROR_HPP
#define MRDOX_LIB_SUPPORT_ERROR_HPP

#include <mrdox/Support/Error.hpp>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>
#include <functional>
#include <utility>

namespace clang {
namespace mrdox {

inline Error toError(llvm::Error err)
{
    if(! err)
        return {};
    return Error(toString(std::move(err)));
}

//------------------------------------------------

namespace report {

/** Helper for ensuring correct grammar in expository output.
*/
template<std::integral T>
class numberOf
{
    T t_;
    std::string_view one_;
    std::string_view notOne_;

public:
    numberOf(
        T t,
        std::string_view one,
        std::string_view notOne) noexcept
        : t_(t)
        , one_(one)
        , notOne_(notOne)
    {
    }

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        numberOf const& u)
    {
        os << u.t_ << ' ';
        if(u.t_ == 1)
            os << u.one_;
        else
            os << u.notOne_;
        return os;
    }
};

template<class T>
numberOf(T, std::string_view, std::string_view) -> numberOf<T>;

//------------------------------------------------

/** Helper for inserting separators into a list.
*/
class separator
{
    char c0_ = ',';
    char c_ = 0;

public:
    constexpr separator();
    constexpr explicit separator(char c)
        : c0_(c)
    {
    }

    friend
    llvm::raw_ostream&
    operator<<(
        llvm::raw_ostream& os,
        separator& u)
    {
        if(u.c_)
            return os << u.c_ << ' ';
        u.c_ = u.c0_;
        return os;
    }
};

//------------------------------------------------

/** Return a level from an integer.
*/
MRDOX_DECL
Level
getLevel(unsigned level);

/** Formatted reporting to a live stream.

    A trailing newline will be added automatically.
*/
MRDOX_DECL
void
call_impl(
    Level level,
    std::function<void(llvm::raw_ostream&)> f,
    source_location const* loc);

/** Formatted reporting to a live stream.

    A trailing newline will be added automatically.
*/
inline void
call(
    Level level,
    std::function<void(llvm::raw_ostream&)> f,
    source_location const& loc =
        source_location::current())
{
    call_impl(level, std::move(f), &loc);
}

} // report

} // mrdox
} // clang

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

#endif
