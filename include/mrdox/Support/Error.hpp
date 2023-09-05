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
#include <mrdox/Support/source_location.hpp>
#include <fmt/format.h>
#include <exception>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

//------------------------------------------------

/** Parameter type that adds a source location to a value.
*/
template<class T>
struct Located
{
    T value;
    source_location where;

    template<class Arg>
    requires std::is_constructible_v<T, Arg>
    Located(
        Arg&& arg,
        source_location const& loc =
            source_location::current())
        : value(std::forward<Arg>(arg))
        , where(loc)
    {
    }
};

//------------------------------------------------
//
// Error
//
//------------------------------------------------

class Exception;

/** Holds the description of an error, or success.
*/
class [[nodiscard]] MRDOX_DECL
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

        A default constructed error is
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
        or if all of the errors in the list indicate
        success, then newly constructed object will
        indicate success.
    */
    Error(
        std::vector<Error> const& errors,
        source_location loc =
            source_location::current());

    /** Return true if this holds an error.
    */
    constexpr bool
    failed() const noexcept
    {
        return ! message_.empty();
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

    /** Throw Exception(*this), or do nothing if no failure.
    */
    void maybeThrow() const&
    {
        if(! failed())
            return;
        Throw();
    }

    /** Throw Exception(std::move(*this)), or do nothing if no failure.
    */
    void maybeThrow() &&
    {
        if(! failed())
            return;
        Throw();
    }

    constexpr void swap(Error& rhs) noexcept
    {
        using std::swap;
        swap(message_, rhs.message_);
        swap(reason_, rhs.reason_);
        swap(loc_, rhs.loc_);
    }

    friend constexpr void swap(
        Error& lhs, Error& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    /** Return a value indicating success.
    */
    static Error success() noexcept;
};

//------------------------------------------------
//
// Exception
//
//------------------------------------------------

/** Type of all exceptions thrown by the API.
*/
class MRDOX_DECL
    Exception final : public std::exception
{
    Error err_;

public:
    /** Constructor.
    */
    explicit
    Exception(
        Error err) noexcept
        : err_(std::move(err))
    {
    }

    /** Return the Error stored in the exception.
    */
    Error const&
    error() const noexcept
    {
        return err_;
    }

    /** Return a null-terminated error string.
    */
    char const*
    what() const noexcept override
    {
        return err_.message().c_str();
    }
};

//------------------------------------------------
//
// Expected
//
//------------------------------------------------

/** A container holding an error or a value.
*/
template<class T>
class [[nodiscard]]
    Expected
{
    union
    {
        T v_;
        Error e_;
    };
    bool has_error_;

#if 0
    static_assert(! std::is_reference_v<T>);
    static_assert(! std::convertible_to<T, Error>);
    static_assert(std::move_constructible<T>);
    static_assert(std::is_nothrow_move_constructible_v<T>);
    static_assert(std::is_nothrow_move_constructible_v<Error>);
#endif

public:
    using value_type = T;
    using error_type = Error;

    Expected(Expected const&) = delete;
    Expected& operator=(Expected const&) = delete;

    ~Expected();
    Expected(Error err) noexcept;
    Expected(Expected&& other) noexcept;
    Expected& operator=(Expected&& other) noexcept;

    template<class U>
    requires std::convertible_to<U, T>
    Expected(U&& u);

    template<class U>
    requires std::convertible_to<U, T>
    Expected& operator=(Expected<U>&& other) noexcept;

    // observers

    constexpr bool has_value() const noexcept;
    constexpr bool has_error() const noexcept;
    constexpr explicit operator bool() const noexcept;

    // checked value access

    constexpr T& value() &;
    constexpr T value() &&;
    constexpr T const& value() const&;
    constexpr T value() const&& = delete;
    constexpr T release() noexcept;

    // unchecked access

    constexpr T* operator->() noexcept;
    constexpr T& operator*() & noexcept;
    constexpr T operator*() &&;

    constexpr T const* operator->() const noexcept;
    constexpr T const& operator*() const& noexcept;
    constexpr T operator*() const&& noexcept = delete;

    // error access

    Error error() const& noexcept;
    Error error() && noexcept;

    // swap

    void swap(Expected& rhs) noexcept;

    friend void swap(
        Expected& lhs, Expected& rhs) noexcept
    {
        lhs.swap(rhs);
    }
};

//------------------------------------------------
//
// SourceLocation
//
//------------------------------------------------

/** A source location with filename prettification.
*/
class MRDOX_DECL
    SourceLocation
{
    std::string_view file_;
    std::uint_least32_t line_;
    std::uint_least32_t col_;
    std::string_view func_;

public:
    SourceLocation(
        source_location const& loc) noexcept;

    std::string_view file_name() const noexcept
    {
        return file_;
    }

    std::uint_least32_t line() const noexcept
    {
        return line_;
    }

    std::uint_least32_t column() const noexcept
    {
        return col_;
    }

    std::string_view function_name() const noexcept
    {
        return func_;
    }
};

//------------------------------------------------
//
// Implementation
//
//------------------------------------------------

template<class... Args>
struct FormatString
{
    template<class T>
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

    std::string_view fs;
    source_location loc;
};

inline
Error
Error::
success() noexcept
{
    return Error();
}

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
    fmt::vformat_to(
        std::back_inserter(s),
        fs.fs, fmt::make_format_args(args...));
    return Error(std::move(s), fs.loc);
}

//------------------------------------------------

template<class T>
Expected<T>::
~Expected()
{
    if(has_error_)
        std::destroy_at(&e_);
    else
        std::destroy_at(&v_);
}

template<class T>
Expected<T>::
Expected(
    Error err) noexcept
    : has_error_(true)
{
    MRDOX_ASSERT(err.failed());
    std::construct_at(&e_, std::move(err));
}

template<class T>
Expected<T>::
Expected(
    Expected&& other) noexcept
    : has_error_(other.has_error_)
{
    if(other.has_error_)
        std::construct_at(&e_, std::move(other.e_));
    else
        std::construct_at(&v_, std::move(other.v_));
}

template<class T>
auto
Expected<T>::
operator=(
    Expected&& other) noexcept ->
        Expected&
{
    if(this != &other)
    {
        std::destroy_at(this);
        std::construct_at(this, std::move(other));
    }
    return *this;
}

template<class T>
template<class U>
requires std::convertible_to<U, T>
Expected<T>::
Expected(
    U&& u)
    : has_error_(false)
{
    static_assert(std::is_convertible_v<U, T>);
    static_assert(!std::convertible_to<U, Error>);

    std::construct_at(&v_, std::forward<U>(u));
}

template<class T>
template<class U>
requires std::convertible_to<U, T>
auto
Expected<T>::
operator=(
    Expected<U>&& other) noexcept ->
        Expected&
{
    static_assert(std::is_nothrow_convertible_v<U, T>);
    static_assert(! std::convertible_to<U, Error>);

    Expected temp(std::move(other));
    temp.swap(*this);
    return *this;
}

template<class T>
constexpr
bool
Expected<T>::
has_value() const noexcept
{
    return ! has_error_;
}

template<class T>
constexpr
bool
Expected<T>::
has_error() const noexcept
{
    return has_error_;
}

template<class T>
constexpr
Expected<T>::
operator bool() const noexcept
{
    return ! has_error_;
}

template<class T>
constexpr
T&
Expected<T>::
value() &
{
    if(has_value())
        return v_;
    e_.Throw();
}

template<class T>
constexpr
T const&
Expected<T>::
value() const&
{
    if(has_value())
        return v_;
    e_.Throw();
}

template<class T>
constexpr
T
Expected<T>::
value() &&
{
    return std::move(value());
}

template<class T>
constexpr
T
Expected<T>::
release() noexcept
{
    return std::move(value());
}

template<class T>
constexpr
T*
Expected<T>::
operator->() noexcept
{
    return &v_;
}

template<class T>
constexpr
T&
Expected<T>::
operator*() & noexcept
{
    auto const p = operator->();
    MRDOX_ASSERT(p != nullptr);
    return *p;
}

template<class T>
constexpr
T
Expected<T>::
operator*() &&
{
    return std::move(**this);
}

template<class T>
Error
Expected<T>::
error() && noexcept
{
    if(has_error())
        return std::move(e_);
    return Error::success();
}

template<class T>
constexpr
T const*
Expected<T>::
operator->() const noexcept
{
    return &v_;
}

template<class T>
constexpr
T const&
Expected<T>::
operator*() const& noexcept
{
    auto const p = operator->();
    MRDOX_ASSERT(p != nullptr);
    return *p;
}

template<class T>
Error
Expected<T>::
error() const& noexcept
{
    if(has_error())
        return e_;
    return Error::success();
}

template<class T>
void
Expected<T>::
swap(Expected& rhs) noexcept
{
    using std::swap;
    if(has_error_)
    {
        if(rhs.has_error)
        {
            swap(e_, rhs.e_);
            return;
        }
        Error err(std::move(e_));
        std::destroy_at(&e_);
        std::construct_at(&v_, std::move(rhs.v_));
        std::destroy_at(&rhs.v_);
        std::construct_at(&rhs.e_, std::move(err));
        swap(has_error_, rhs.has_error_);
        return;
    }
    if(! rhs.has_error)
    {
        swap(v_, rhs.v_);
        return;
    }
    Error err(std::move(rhs.e_));
    std::destroy_at(&rhs.e_);
    std::construct_at(&rhs.v_, std::move(v_));
    std::destroy_at(&v_);
    std::construct_at(&e_, std::move(err));
    swap(has_error_, rhs.has_error_);
}

//------------------------------------------------
//
// Reporting
//
//------------------------------------------------

namespace report {

/** Severity levels attached to reported messags.
*/
enum class Level
{
    debug = 0,
    info,
    warn,
    error,
    fatal
};

/** Provides statistics on the number of reported messages.
*/
struct Results
{
    std::size_t debugCount;
    std::size_t infoCount;
    std::size_t warnCount;
    std::size_t errorCount;
    std::size_t fatalCount;
};

/** Holds current statistics on reported messages.
*/
extern
MRDOX_DECL
Results
results;

/** Set the minimum threshold level for reporting.

    Messages below this level will not be printed.
    A value of 5 will suppress all messages. Note
    that messages will still be counted towards
    result totals even if they are not displayed.
*/
MRDOX_DECL
void
setMinimumLevel(
    Level level) noexcept;

/** Report a message to the console.

    @param text The message to print. A
    trailing newline will be added to the
    message automatically.
*/
MRDOX_DECL
void
print(
    std::string const& text);

/** Report a message to the console.

    @param level 0 to 4 The severity of the
    report. 0 is debug and 4 is fatal.

    @param text The message to print. A
    trailing newline will be added to the
    message automatically.

    @param loc The source location of the report.
    If this value is null, no location is printed.
*/
MRDOX_DECL
void
print(
    Level level,
    std::string const& text,
    source_location const* loc = nullptr);

/** Format a message to the console.

    @param level 0 to 4 The severity of the
    report. 0 is debug and 4 is fatal.

    @param fs The format string.

    @param args... Optional additional arguments
    used to format a message to print. A trailing
    newline will be added to the message
    automatically.
*/
template<class... Args>
void
format(
    Level level,
    Located<std::string_view> fs,
    Args&&... args)
{
    return print(
        level,
        fmt::vformat(
            fs.value,
            fmt::make_format_args(args...)),
        &fs.where);
}

/** Report a message to the console.
*/
template<class... Args>
void
debug(
    Located<std::string_view> format,
    Args&&... args)
{
    return print(
        Level::debug,
        fmt::vformat(
            format.value,
            fmt::make_format_args(args...)),
        &format.where);
}

/** Report a message to the console.
*/
template<class... Args>
void
info(
    Located<std::string_view> format,
    Args&&... args)
{
    return print(
        Level::info,
        fmt::vformat(
            format.value,
            fmt::make_format_args(args...)),
        &format.where);
}

/** Report a message to the console.
*/
template<class... Args>
void
warn(
    Located<std::string_view> format,
    Args&&... args)
{
    return print(
        Level::warn,
        fmt::vformat(
            format.value,
            fmt::make_format_args(args...)),
        &format.where);
}

/** Report a message to the console.
*/
template<class... Args>
void
error(
    Located<std::string_view> format,
    Args&&... args)
{
    return print(
        Level::error,
        fmt::vformat(
            format.value,
            fmt::make_format_args(args...)),
        &format.where);
}

/** Report a message to the console.
*/
template<class... Args>
void
fatal(
    Located<std::string_view> format,
    Args&&... args)
{
    return print(
        Level::fatal,
        fmt::vformat(
            format.value,
            fmt::make_format_args(args...)),
        &format.where);
}

} // report

} // mrdox
} // clang

//------------------------------------------------

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
        return fmt::formatter<std::string_view>::format(err.reason(), ctx);
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
