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

#ifndef MRDOX_API_SUPPORT_ERROR_HPP
#define MRDOX_API_SUPPORT_ERROR_HPP

#include <mrdox/Platform.hpp>
#include <fmt/format.h>
#include <exception>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace clang {
namespace mrdox {

/** Holds the description of an error, or success.
*/
class [[nodiscard]] MRDOX_DECL
    Error final : public std::exception
{
    std::string text_;

public:
    /** Constructor.

        A default constructed error is
        equivalent to success.
    */
    Error() = default;

    /** Constructor.
    */
    Error(Error&&) = default;

    /** Constructor.
    */
    Error(Error const&) = default;

    /** Constructor.
    */
    Error& operator=(Error&&) = default;

    /** Assignment.
    */
    Error& operator=(Error const&) = default;

    /** Constructor.

        @param text A message describing the error.
        An empty string indicates success.
    */
    explicit
    Error(
        std::string_view text)
        : text_(text)
    {
    }

    /** Constructor.

        @param fs The format string. An empty
        string indicates success.

        @param args The arguments to use
        with the format string.
    */
    template<class... Args>
    explicit
    Error(
        std::string_view fs,
        Args&&... args)
        : text_(fmt::vformat(fs,
            fmt::make_format_args(
                std::forward<Args>(args)...)))
    {
    }

    /** Constructor.

        @param ec The error code.
    */
    explicit
    Error(
        std::error_code const& ec)
        : Error(ec
            ? std::string_view(ec.message())
            : std::string_view())
    {
    }

    /** Return true if this holds an error.
    */
    bool failed() const noexcept
    {
        return ! text_.empty();
    }

    /** Return true if this holds an error.
    */
    explicit
    operator bool() const noexcept
    {
        return failed();
    }

    /** Return the error string.
    */
    std::string_view
    message() const noexcept
    {
        return text_;
    }

    /** Return true if this equals other.
    */
    bool
    operator==(Error const& other) const noexcept
    {
        return text_ == other.text_;
    }

    /** Return a null-terminated error string.
    */
    char const*
    what() const noexcept override
    {
        return text_.c_str();
    }

    /** Return a value indicating success.
    */
    static
    Error
    success() noexcept;
};

inline
Error
Error::
success() noexcept
{
    return Error();
}

} // mrdox
} // clang

template<>
struct std::hash<::clang::mrdox::Error>
{
    std::size_t operator()(
        ::clang::mrdox::Error const& err) const noexcept
    {
        return std::hash<std::string_view>()(err.message());
    }
};

//------------------------------------------------

template<>
struct fmt::formatter<clang::mrdox::Error>
    : fmt::formatter<std::string_view>
{
    auto format(
        clang::mrdox::Error const& err,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string_view>::format(err.message(), ctx);
    }
};

template<>
struct fmt::formatter<std::error_code>
    : fmt::formatter<std::string_view>
{
    auto format(
        std::error_code const& ec,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string_view>::format(ec.message(), ctx);
    }
};

#endif
