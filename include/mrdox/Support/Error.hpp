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
#include <mrdox/Support/Format.hpp>
#include <exception>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

/** Holds the description of an error, or success.
*/
class [[nodiscard]] MRDOX_DECL
    Error final : public std::exception
{
    std::string text_;

    static
    std::string
    appendSourceLocation(
        std::string&&, std::source_location const&);

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

        @param s The text of the error. This
        must not be empty.
    */
    explicit
    Error(
        std::string s,
        std::source_location loc =
            std::source_location::current())
        : text_(appendSourceLocation(std::move(s), loc))
    {
        MRDOX_ASSERT(! text_.empty());
    }

    /** Constructor.

        @param ec The error code.
    */
    explicit
    Error(
        std::error_code const& ec,
        std::source_location loc =
            std::source_location::current())
    {
        if(! ec)
            return;
        text_ = appendSourceLocation(ec.message(), loc);
    }

    /** Constructor.

        @param fs The format string. This
        must not be empty.

        @param args Zero or more values to
        substitute into the format string.
    */
    template<class... Args>
    explicit
    Error(
        FormatString<std::type_identity_t<Args>...> fs,
        Args&&... args)
        : Error(
            [&]
            {
                std::string s;
                fmt::vformat_to(
                    std::back_inserter(s),
                    fs.fs, fmt::make_format_args(
                        std::forward<Args>(args)...));
                return s;
            }(), fs.loc)
    {
    }

    explicit
    Error(std::vector<Error> const& errors);

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
