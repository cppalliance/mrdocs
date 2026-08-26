//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_ERROR_ERROR_HPP
#define MRDOCS_API_SUPPORT_ERROR_ERROR_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/polyfill/source_location.hpp>
#include <exception>
#include <format>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace mrdocs {

//------------------------------------------------
//
// Error
//
//------------------------------------------------

class Exception;

namespace dom { class Error; }
namespace handlebars { struct Error; }

/** Holds the description of an error, or success.
*/
class MRDOCS_DECL
    Error final
{
    std::string where_;
    std::string reason_;
    std::string message_;
    source_location loc_;

    static std::string formatWhere(source_location const&);
    static std::string formatMessage(
        std::string_view const&, std::string_view const&);
public:
    /** Constructor.

        A default-constructed error is
        equivalent to success.
    */
    Error() noexcept = default;

    /** Constructor.
    */
    Error(Error&&) noexcept = default;

    /** Constructor.
    */
    Error(Error const&) = default;

    /** Constructor.
    */
    Error& operator=(Error&&) noexcept = default;

    /** Assignment.
    */
    Error& operator=(Error const&) = default;

    /** Constructor.

        @param reason A string indicating the
        cause of the failure. This must not
        be empty.

        @param loc The source location where
        the error occurred.
    */
    explicit
    Error(
        std::string reason,
        source_location loc =
            source_location::current());

    /** Constructor.

        @param ec The error code.
        @param loc The source location where
        the error occurred.
    */
    explicit
    Error(
        std::error_code const& ec,
        source_location loc =
            source_location::current());

    /** Constructor.

        The constructed object will always
        indicate a failure, even if the message
        in the exception is empty.
    */
    explicit Error(std::exception const& ex);

    /** Constructor.

        This constructs a new error from a list
        of zero or more errors. If the list is empty,
        or if all the errors in the list indicate
        success, then newly constructed object will
        indicate success.

        @param errors The list of errors to combine.
        @param loc The source location where
        the error occurred.
    */
    Error(
        std::vector<Error> const& errors,
        source_location loc =
            source_location::current());

    /** Constructor from a library-local error.

        Implicit on purpose: errors flow upward from the std-only library
        errors (e.g. `dom::Error`) to `mrdocs::Error` at library boundaries,
        so an `Expected<T, dom::Error>` converts to `Expected<T, Error>`
        with no caller changes.
    */
    Error(dom::Error const& e);

    /** Constructor from a library-local error.

        Implicit on purpose: errors flow upward from the handlebars library
        error to `mrdocs::Error` at library boundaries, so an
        `Expected<T, handlebars::Error>` converts to `Expected<T, Error>`
        with no caller changes.
    */
    Error(handlebars::Error const& e);

    /** Return true if this holds an error.
    */
    constexpr bool
    failed() const noexcept
    {
        return !message_.empty();
    }

    /** Return true if this holds an error.
    */
    constexpr explicit
    operator bool() const noexcept
    {
        return failed();
    }

    /** Return the location string.
    */
    constexpr std::string const&
    where() const noexcept
    {
        return where_;
    }

    /** Return the reason string.
    */
    constexpr std::string const&
    reason() const noexcept
    {
        return reason_;
    }

    /** Return the error string.
    */
    constexpr std::string const&
    message() const noexcept
    {
        return message_;
    }

    /** Return the source location.
    */
    constexpr source_location const&
    location() const noexcept
    {
        return loc_;
    }

    /** Return true if this equals rhs.
    */
    constexpr bool
    operator==(Error const& rhs) const noexcept
    {
        return message_ == rhs.message_;
    }

    /** Throw Exception(*this)

        @pre this->failed()
    */
    [[noreturn]] void Throw() const&;

    /** Throw Exception(std::move(*this))

        @pre this->failed()
    */
    [[noreturn]] void Throw() &&;

    /** Swap the contents with another error.
    */
    constexpr
    void
    swap(Error& rhs) noexcept
    {
        using std::swap;
        swap(message_, rhs.message_);
        swap(reason_, rhs.reason_);
        swap(loc_, rhs.loc_);
    }

    /** Swap the contents of two @ref Error objects.
        @param lhs The first error to swap.
        @param rhs The second error to swap.
    */
    friend
    constexpr
    void
    swap(Error& lhs, Error& rhs) noexcept
    {
        lhs.swap(rhs);
    }
};


} // mrdocs

template<>
struct std::hash<::mrdocs::Error>
{
    std::size_t operator()(
        ::mrdocs::Error const& err) const noexcept
    {
        return std::hash<std::string_view>()(err.message());
    }
};

template <>
struct std::formatter<mrdocs::Error> : std::formatter<std::string_view> {
  template <class FmtContext>
  auto format(mrdocs::Error const &err, FmtContext &ctx) const {
    return std::formatter<std::string_view>::format(err.message(), ctx);
  }
};

template <>
struct std::formatter<std::error_code> : std::formatter<std::string_view> {
  template <class FmtContext>
  auto format(std::error_code const &ec, FmtContext &ctx) const {
    return std::formatter<std::string_view>::format(ec.message(), ctx);
  }
};

namespace mrdocs {

/** A source location with filename prettification.
*/
class MRDOCS_DECL
    SourceLocation
{
    std::string_view file_;
    std::uint_least32_t line_;
    std::uint_least32_t col_;
    std::string_view func_;

public:
    /** Build a location wrapper from a `source_location`.
    */
    SourceLocation(
        source_location const& loc) noexcept;

    /** File name associated with the location.
        @return Path of the source file.
    */
    std::string_view file_name() const noexcept
    {
        return file_;
    }

    /** Line number (1-based) within the file.
        @return One-based line index.
    */
    std::uint_least32_t line() const noexcept
    {
        return line_;
    }

    /** Column number (1-based) within the file.
        @return One-based column index.
    */
    std::uint_least32_t column() const noexcept
    {
        return col_;
    }

    /** Function name captured at the location.
        @return Name of the function where the location was recorded.
    */
    std::string_view function_name() const noexcept
    {
        return func_;
    }
};

/** A format string with source location.
*/
template<class... Args>
struct FormatString
{
    /** Capture a format string and its originating location.

        @param fs_ The format string to copy.
        @param loc_ Source location where formatting is requested.
    */
    template <class T>
    FormatString(
        T const& fs_,
        source_location loc_ =
            source_location::current())
        : fs(fs_)
        , loc(loc_)
    {
        static_assert(std::is_constructible_v<
            std::string_view, T const&>);
    }

    /** Format string view.
    */
    std::string_view fs;

    /** Originating source location for diagnostics.
    */
    source_location loc;
};

/** Return a formatted error.

    @param fs The format string. This
    must not be empty.

    @param args Zero or more values to
    substitute into the format string.
*/
template<class... Args>
Error
formatError(
    FormatString<std::type_identity_t<Args>...> fs,
    Args&&... args)
{
    std::string s;
    std::vformat_to(std::back_inserter(s), fs.fs,
                    std::make_format_args(args...));
    return Error(std::move(s), fs.loc);
}

}

#include <mrdocs/Support/Error/Exception.hpp>

#endif
