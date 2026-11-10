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

#ifndef MRDOCS_API_SUPPORT_REPORTER_HPP
#define MRDOCS_API_SUPPORT_REPORTER_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/polyfill/source_location.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <atomic>
#include <exception>
#include <format>
#include <functional>
#include <iterator>
#include <memory>
#include <mutex>
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

/** Severity levels attached to reported messags.
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
struct MRDOCS_VISIBLE Results
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

/** The reporter of one run.

    A reporter formats, prints and counts the messages of a run. It owns
    the minimum level, whether an error shows where in the source it came
    from, the `warn-as-error` setting and the counters, so two reporters
    never see each other's settings or totals. A @ref Session
    owns the reporter of its run and returns it from @ref Session::log.
    The core and the finalizers receive it from the session, and a
    plugin through the `report` field of its context.

    `warn-as-error` applies to every warning. When it is set, @ref warn
    reports at @ref Level::error, so a warning of the core, of a script
    (`ctx.report.warn`) and of a plugin (`ctx.report->warn`) all fail the
    run, and the run counts them as errors. The message is still printed
    where it is reported, with the others.

    Every function is safe to call from several threads at once. The
    reporters of a process share one lock for the console, so the lines
    of two runs never mix.

    @code
    session.log().warn("{} has no brief", symbol.Name);
    if (session.log().results().errorCount > 0)
    {
        return EXIT_FAILURE;
    }
    @endcode
*/
class MRDOCS_DECL
    Reporter
{
public:
    /** Constructor.

        The reporter prints messages of every level from
        @ref Level::debug up, prints the source location with errors,
        and does not treat warnings as errors.
    */
    Reporter() noexcept;

    /** Constructor.

        @param minimumLevel The lowest level that is printed.
    */
    explicit Reporter(Level minimumLevel) noexcept;

    /** Destructor.
    */
    ~Reporter();

    Reporter(Reporter const&) = delete;
    Reporter& operator=(Reporter const&) = delete;

    /** Set the minimum threshold level for reporting.

        Messages below this level will not be printed.
        Note that messages will still be counted towards
        result totals even if they are not displayed.

        @param level The lowest level that is printed.
    */
    void
    setMinimumLevel(Level level) noexcept;

    /** Get the minimum threshold level for reporting.

        @return Current minimum Level that will be emitted.
    */
    Level
    getMinimumLevel() const noexcept;

    /** Set whether errors carry the location they were reported from.

        If true, source location information will be
        printed with errors and fatal messages.

        @param b true to enable source location
        information, false to disable it. The default
        value is true.
    */
    void
    setSourceLocationWarnings(bool b) noexcept;

    /** Set whether the run treats warnings as errors.

        @param b The `warn-as-error` setting of the run. The default
        value is false.
    */
    void
    setWarnAsError(bool b) noexcept;

    /** Return whether the run treats warnings as errors.

        @ref warn reports at @ref Level::error when this is true.

        @return The `warn-as-error` setting of the run.
    */
    bool
    warnAsError() const noexcept;

    /** Return the number of messages reported so far, by level.

        @return A copy of the counters.
    */
    Results
    results() const;

    /** Print a message to the console.

        The message is neither counted nor filtered by level.

        @param text The message to print. A
        trailing newline will be added to the
        message automatically.
    */
    void
    print(std::string const& text);

    /** Report a message to the console.

        @param level The severity of the report.

        @param text The message to print. A
        trailing newline will be added to the
        message automatically.

        @param loc The source location of the report.
        If this value is null, no location is printed.

        @param e The error the message is about, if any.
        Its location is printed with the source location.
    */
    void
    print(
        Level level,
        std::string const& text,
        source_location const* loc = nullptr,
        Error const* e = nullptr);

    /** Format a message to the console.

        @param level The severity of the report.

        @param fs The format string.

        @param args Optional additional arguments
        used to format a message to print. A trailing
        newline will be added to the message
        automatically.
    */
    template <class... Args>
    void
    log(
        Level level,
        Located<std::string_view> fs,
        Args&&... args)
    {
        if constexpr (sizeof...(Args) == 0)
        {
            print(level, std::string(fs.value), &fs.where);
        }
        else
        {
            logArgs(level, &fs.where, fs.value, std::forward<Args>(args)...);
        }
    }

    /** Emit a trace-level diagnostic (verbose, off by default).
        @param format fmt-style format string.
        @param args Arguments substituted into the format string.
    */
    template <class... Args>
    void
    trace(
        Located<std::string_view> format,
        Args&&... args)
    {
        log(Level::trace, format, std::forward<Args>(args)...);
    }

    /** Emit a debug-level diagnostic for troubleshooting.
        @param format fmt-style format string.
        @param args Arguments substituted into the format string.
    */
    template <class... Args>
    void
    debug(
        Located<std::string_view> format,
        Args&&... args)
    {
        log(Level::debug, format, std::forward<Args>(args)...);
    }

    /** Emit an informational message for users.
        @param format fmt-style format string.
        @param args Arguments substituted into the format string.
    */
    template <class... Args>
    void
    info(
        Located<std::string_view> format,
        Args&&... args)
    {
        log(Level::info, format, std::forward<Args>(args)...);
    }

    /** Emit a warning.

        Under `warn-as-error` the message is printed and counted as an
        error, so the run fails. It is printed as a warning is, without the
        bug-report details an internal error carries. Otherwise it is a
        warning that does not stop execution.

        @param format fmt-style format string.
        @param args Arguments substituted into the format string.
    */
    template <class... Args>
    void
    warn(
        Located<std::string_view> format,
        Args&&... args)
    {
        if (!warnAsError())
        {
            log(Level::warn, format, std::forward<Args>(args)...);
            return;
        }
        // The message is a warning promoted to an error, not a bug, so it
        // is printed without a source location.
        if constexpr (sizeof...(Args) == 0)
        {
            print(Level::error, std::string(format.value));
        }
        else
        {
            logArgs(
                Level::error, nullptr, format.value,
                std::forward<Args>(args)...);
        }
    }

    /** Emit an error that indicates failure but allows continuation.
        @param format fmt-style format string.
        @param args Arguments substituted into the format string.
    */
    template <class... Args>
    void
    error(
        Located<std::string_view> format,
        Args&&... args)
    {
        log(Level::error, format, std::forward<Args>(args)...);
    }

    /** Emit a fatal error and mark the report as the highest severity.
        @param format fmt-style format string.
        @param args Arguments substituted into the format string.
    */
    template <class... Args>
    void
    fatal(
        Located<std::string_view> format,
        Args&&... args)
    {
        log(Level::fatal, format, std::forward<Args>(args)...);
    }

private:
    // Format and print a message. `where` is the source location printed
    // with an error, or null to print none.
    template <class Arg0, class... Args>
    void
    logArgs(
        Level level,
        source_location const* where,
        std::string_view fs,
        Arg0&& arg0,
        Args&&... args)
    {
        if constexpr (std::same_as<std::decay_t<Arg0>, Error>)
        {
            // When the message is an error, the information relevant to
            // the user is split from the information relevant for bug
            // tracking so that users can understand the message.
            std::string str = std::vformat(
                fs, std::make_format_args(arg0.reason(), args...));
            print(level, str, where, &arg0);
        }
        else
        {
            std::string str = std::vformat(
                fs, std::make_format_args(arg0, args...));
            print(level, str, where);
        }
    }

    mutable std::mutex mutex_;
    std::atomic<Level> level_;
    std::atomic<bool> sourceLocationWarnings_{true};
    std::atomic<bool> warnAsError_{false};
    Results results_{};
};

} // mrdocs::report

#endif
