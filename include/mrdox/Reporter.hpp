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

#ifndef MRDOX_REPORTER_HPP
#define MRDOX_REPORTER_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Error.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/Mutex.h>
#include <llvm/Support/raw_ostream.h>
#include <cassert>
#include <source_location>
#include <string>
#include <system_error>
#include <type_traits>

namespace clang {
namespace mrdox {

/** Used to check and report errors uniformly.
*/
class Reporter
{
public:
    /** Return a suitable exit code.
    */
    int getExitCode() const noexcept;

    /** Print formatted output.

        @param arg0, args The values to write.
    */
    template<class Arg0, class... Args>
    void
    print(
        Arg0&& arg, Args&&... args);

    /** Report the failure of an action.

        @param arg0, args The values to write.
    */
    template<class Arg0, class... Args>
    void
    failed(
        Arg0&& arg, Args&&... args);

    /** Return true and report a message if an error is indicated.

        The error object `e` is inspected for a failure
        condition. If `e` does not contain a failure,
        then this function returns `false`. Otherwise
        the function emits a diagnostic message to the
        error channel and returns `true`.

        @par Output
        The diagnostic message will always have the form:
        @code
        error: couldn't %1 because %2
        @endcode
        The first string is formed from the `actionFormat`
        parameter and the variadic list of arguments,
        while the second string is formed from the error
        object.

        If source location information is available, it
        is reported on additional indented lines following
        the diagnostic message.

        @par Effects
        The number of errors seen by the reporter
        is incremented by one.

        @return true if `e` indicates a failure.

        @param e The error object to inspect.

        @param arg0, args The values to write.
    */
    /** @{ */
    template<
        class E,
        class Arg0, class... Args>
    [[nodiscard]]
    bool
    error(
        E&& e,
        Arg0&& arg0,
        Args&&... args);

    template<
        class E,
        class Arg0, class... Args>
    [[nodiscard]]
    bool
    error(
        E& e,
        Arg0&& arg0,
        Args&&... args);
    /** @} */

    //--------------------------------------------

    /** Increment the count of errors.

        @par Thread Safety
        May be called concurrently.
    */
    void
    reportError();

private:
    //--------------------------------------------

    template<class T>
    static bool isFailure(llvm::Expected<T>&& e) noexcept
    {
        return ! e.operator bool();
    }

    template<class T>
    static bool isFailure(llvm::ErrorOr<T>&& e) noexcept
    {
        return ! e.operator bool();
    }

    static bool isFailure(llvm::Error&& e) noexcept
    {
        return e.operator bool();
    }

    static bool isFailure(std::error_code&& ec) noexcept
    {
        return ec.operator bool();
    }

    void
    threadSafePrint(
        llvm::raw_fd_ostream& os,
        llvm::StringRef s,
        std::size_t* n = nullptr);

    static std::string& temp_string();

private:
    llvm::sys::Mutex mutable m_;
    std::size_t errorCount_ = 0;
};

//------------------------------------------------

template<
    class Arg0, class... Args>
void
Reporter::
print(
    Arg0&& arg,
    Args&&... args)
{
    auto& temp = temp_string();
    temp.clear();
    {
        llvm::raw_string_ostream os(temp);
        os << nice(std::forward<Arg0>(arg));
        if constexpr(sizeof...(args) > 0)
            (os << ... << nice(std::forward<Args>(args)));
    }
    threadSafePrint(llvm::outs(), temp, nullptr);
}

template<
    class Arg0, class... Args>
void
Reporter::
failed(
    Arg0&& arg,
    Args&&... args)
{
    auto& temp = temp_string();
    temp.clear();
    {
        llvm::raw_string_ostream os(temp);
        os << "error: Couldn't ";
        os << nice(std::forward<Arg0>(arg));
        if constexpr(sizeof...(args) > 0)
            (os << ... << nice(std::forward<Args>(args)));
        os << ".";
    }
    threadSafePrint(llvm::errs(), temp, &errorCount_);
}

template<
    class E,
    class Arg0, class... Args>
bool
Reporter::
error(
    E&& e,
    Arg0&& arg0,
    Args&&... args)
{
    if(! isFailure(std::forward<E>(e)))
        return false;
    auto& temp = temp_string();
    temp.clear();
    {
        llvm::raw_string_ostream os(temp);
        os << "error: Couldn't ";
        os << nice(std::forward<Arg0>(arg0));
        if constexpr(sizeof...(args) > 0)
            (os << ... << nice(std::forward<Args>(args)));
        os << " because " << nice(std::forward<E>(e)) << '.';
    }
    threadSafePrint(llvm::errs(), temp, &errorCount_);
    return true;
}

template<
    class E,
    class Arg0, class... Args>
bool
Reporter::
error(
    E& e,
    Arg0&& arg0,
    Args&&... args)
{
    return error(
        std::move(e),
        std::forward<Arg0>(arg0),
        std::forward<Args>(args)...);
}

} // mrdox
} // clang

#endif
