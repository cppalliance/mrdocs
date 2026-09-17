//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_REPORT_HPP
#define MRDOCS_API_SUPPORT_REPORT_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/polyfill/source_location.hpp>
#include <exception>
#include <format>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

/** Reporting utilities (messages, statistics, sinks).

    The `report` namespace owns severity enums, message structs, and output
    sinks so tooling and libraries emit diagnostics in a uniform, testable
    format regardless of UI.
*/
namespace mrdocs::report {

/** Severity levels attached to reported messages.
*/
enum class Level
{
    /// Programming trace messages
    trace = 0,
    /// Debug messages
    debug,
    /// Informational messages
    info,
    /// Warning messages
    warn,
    /// Error messages
    error,
    /// Fatal error messages
    fatal
};

/** Provides statistics on the number of reported messages.
*/
struct Results
{
    /** Number of trace-level messages.
    */
    std::size_t traceCount;
    /** Number of debug-level messages.
    */
    std::size_t debugCount;
    /** Number of info-level messages.
    */
    std::size_t infoCount;
    /** Number of warning-level messages.
    */
    std::size_t warnCount;
    /** Number of error-level messages.
    */
    std::size_t errorCount;
    /** Number of fatal-level messages.
    */
    std::size_t fatalCount;
};

/** Holds current statistics on reported messages.
*/
extern
MRDOCS_DECL
Results
results;

/** Set the minimum threshold level for reporting.

    Messages below this level will not be printed.
    A value of 5 will suppress all messages. Note
    that messages will still be counted towards
    result totals even if they are not displayed.
*/
MRDOCS_DECL
void
setMinimumLevel(Level level) noexcept;

MRDOCS_DECL
Level
/** Get the minimum threshold level for reporting.

    @return Current minimum Level that will be emitted.
*/
getMinimumLevel() noexcept;

/** If true, source location information will be
    printed.

    @param b true to enable the bug report details,
    false to disable them. The default value is true.
*/
MRDOCS_DECL
void
setSourceLocationWarnings(bool b) noexcept;

/** Report a message to the console.

    @param text The message to print. A
    trailing newline will be added to the
    message automatically.
*/
MRDOCS_DECL
void
print(
    std::string const& text);

/** Report a message to the console.

    @param level 0 to 4 The severity of the
    report. 0 is debug and 4 is fatal.

    @param text The message to print. A
    trailing newline will be added to the
    message automatically.

    @param loc The source location a bug report
    should carry. If this value is null, the message
    is printed on its own, without the bug report
    details.

    @param e The error the message reports, when
    there is one. Only read when `loc` is not null.
*/
MRDOCS_DECL
void
print(
    Level level,
    std::string const& text,
    source_location const* loc = nullptr,
    Error const* e = nullptr);

/** Parameter type that adds a source location to a value.
*/
template<class T>
struct Located
{
    /** Wrapped value.
    */
    T value;
    /** Source location of the value.
    */
    source_location where;

    /** Construct a Located wrapper.

        @param arg Value to wrap.
        @param loc Source location to associate (defaults to current).
    */
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

/** Internal helpers for report rendering and formatting.

    These are kept in `detail` to isolate formatting glue (color, alignment,
    column widths) from the public reporting API.
*/
namespace detail {
// `withBugDetails` says whether the message reports a MrDocs defect. Only
// then is the location where MrDocs raised it worth printing, along with
// the rest of what a bug report needs. A message about the user's input
// does not contain such info.
template<class Arg0, class... Args>
requires (!std::same_as<std::decay_t<Arg0>, Error>)
void
log_impl(
    Level level,
    bool withBugDetails,
    Located<std::string_view> fs,
    Arg0&& arg0,
    Args&&... args)
{
  std::string str =
      std::vformat(fs.value, std::make_format_args(arg0, args...));
  return print(level, str, withBugDetails ? &fs.where : nullptr);
}

template<class... Args>
void
log_impl(
    Level level,
    bool withBugDetails,
    Located<std::string_view> fs,
    Error const& e,
    Args&&... args)
{
    // The reason is what went wrong; where MrDocs raised it belongs to the
    // bug report details, so the message carries the reason alone.
    std::string str =
        std::vformat(fs.value, std::make_format_args(e.reason(), args...));
    return print(
        level,
        str,
        withBugDetails ? &fs.where : nullptr,
        &e);
}

inline
void
log_impl(
    Level level,
    bool withBugDetails,
    Located<std::string_view> fs)
{
  std::string str(fs.value);
  return print(level, str, withBugDetails ? &fs.where : nullptr);
}
}

/** Format a message to the console.

    @param level 0 to 4 The severity of the
    report. 0 is debug and 4 is fatal.

    @param fs The format string.

    @param args Optional additional arguments
    used to format a message to print. A trailing
    newline will be added to the message
    automatically.
*/
template<class... Args>
void
log(
    Level level,
    Located<std::string_view> fs,
    Args&&... args)
{
    return detail::log_impl(
        level,
        false,
        fs,
        std::forward<Args>(args)...);
}

/** Emit a trace-level diagnostic (verbose, off by default).
    @param format fmt-style format string.
    @param args Arguments substituted into the format string.
*/
template<class... Args>
void
trace(
    Located<std::string_view> format,
    Args&&... args)
{
    return log(Level::trace, format, std::forward<Args>(args)...);
}

/** Emit a debug-level diagnostic for troubleshooting.
    @param format fmt-style format string.
    @param args Arguments substituted into the format string.
*/
template<class... Args>
void
debug(
    Located<std::string_view> format,
    Args&&... args)
{
    return log(Level::debug, format, std::forward<Args>(args)...);
}

/** Emit an informational message for users.
    @param format fmt-style format string.
    @param args Arguments substituted into the format string.
*/
template<class... Args>
void
info(
    Located<std::string_view> format,
    Args&&... args)
{
    return log(Level::info, format, std::forward<Args>(args)...);
}

/** Emit a warning that does not stop execution.
    @param format fmt-style format string.
    @param args Arguments substituted into the format string.
*/
template<class... Args>
void
warn(
    Located<std::string_view> format,
    Args&&... args)
{
    return log(Level::warn, format, std::forward<Args>(args)...);
}

/** Emit an error that indicates failure but allows continuation.
    @param format fmt-style format string.
    @param args Arguments substituted into the format string.
*/
template<class... Args>
void
error(
    Located<std::string_view> format,
    Args&&... args)
{
    return log(Level::error, format, std::forward<Args>(args)...);
}

/** Emit a fatal error and mark the report as the highest severity.
    @param format fmt-style format string.
    @param args Arguments substituted into the format string.
*/
template<class... Args>
void
fatal(
    Located<std::string_view> format,
    Args&&... args)
{
    return log(Level::fatal, format, std::forward<Args>(args)...);
}

/** Emit an error that reports a defect in MrDocs itself.

    The message is followed by the version and the source location a bug
    report needs. Use it where MrDocs reached a state it does not handle.
    A mistake in the user's input is not a bug in MrDocs, so it goes
    through the other reporting functions.

    @param format fmt-style format string.
    @param args Arguments substituted into the format string.
*/
template<class... Args>
void
bug(
    Located<std::string_view> format,
    Args&&... args)
{
    return detail::log_impl(
        Level::error,
        true,
        format,
        std::forward<Args>(args)...);
}

} // mrdocs

#endif
