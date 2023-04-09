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

#include <llvm/Support/Error.h>
#include <llvm/Support/Mutex.h>
#include <llvm/Support/raw_ostream.h>
#include <cassert>
#include <new>
#include <source_location>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// Reporter
//
//------------------------------------------------

/** Used to check and report errors uniformly.
*/
struct Reporter
{
    /** Return a suitable exit code.
    */
    int
    getExitCode() const noexcept
    {
        if(! failed_)
            return EXIT_SUCCESS;
        return EXIT_FAILURE;
    }

    /** Report a non-successful Error
    */
    void
    failed(llvm::Error&& err);

    /** Report the failure of an action.

        @param action   The operation which failed.
        @param e        The object which holds the error.
        @param loc      The location where the report is called.
    */
    template<class E>
    void
    failed(
        llvm::StringRef action,
        E&& e,
        std::source_location const& loc =
            std::source_location::current())
    {
        errs(action, " failed: ", std::move(e), " at ", loc);
    }

    /** Report a unit test failure.

        @par Thread Safety
        May be called concurrently.
    */
    void
    testFailed()
    {
        failed_ = true;
    }

    /** Write a formatted string to outputs.

        @par Thread Safety
        May be called concurrently.
    */
    template<class... Args>
    void
    outs(Args&&... args) const
    {
        llvm::StringRef s = format(std::forward<Args>(args)...);
        std::lock_guard<llvm::sys::Mutex> lock(m_);
        llvm::errs() << s << '\n';
    }

    /** Write a formatted string to errors.

        @par Thread Safety
        May be called concurrently.
    */
    template<class... Args>
    void
    errs(Args&&... args) const
    {
        llvm::StringRef s = format(std::forward<Args>(args)...);
        std::lock_guard<llvm::sys::Mutex> lock(m_);
        llvm::errs() << s << '\n';
    }

    static
    llvm::StringRef
    makeString(
        std::source_location const& loc);

private:
    /** Return a string formatted from arguments.

        @par Thread Safety
        May be called concurrently.
    */
    template<class... Args>
    llvm::StringRef
    format(
        Args&&... args) const
    {
        static thread_local std::string temp;
        temp.clear();
        llvm::raw_string_ostream os(temp);
        format_impl(os, std::forward<Args>(args)...);
        return llvm::StringRef(temp.data(), temp.size());
    }

    /** Format arguments into an output stream.
    */
    template<class Arg0, class... Args>
    void
    format_impl(
        llvm::raw_string_ostream& os,
        Arg0&& arg0,
        Args&&... args) const
    {
        format_one(os, std::forward<Arg0>(arg0));
        if constexpr(sizeof...(args) > 0)
            format_impl(os, std::forward<Args>(args)...);
    }

    template<class T>
    void
    format_one(
        llvm::raw_string_ostream& os,
        T const& t) const
    {
        os << t;
    }

    void
    format_one(
        llvm::raw_string_ostream& os,
        std::error_code const& ec) const
    {
        os << ec.message();
    }

    void
    format_one(
        llvm::raw_string_ostream& os,
        llvm::Error&& err) const
    {
        //err.operator bool(); // VFALCO?
        os << toString(std::move(err));
    }

    template<class T>
    void
    format_one(
        llvm::raw_string_ostream& os,
        llvm::Expected<T>&& ex) const
    {
        format_one(os, ex.takeError());
    }

    template<class T>
    void
    format_one(
        llvm::raw_string_ostream& os,
        llvm::ErrorOr<T>&& eor) const
    {
        format_one(os, eor.getError());
    }

    void
    format_one(
        llvm::raw_string_ostream& os,
        std::source_location const& loc) const
    {
        os << makeString(loc);
    }

private:
    llvm::sys::Mutex mutable m_;
    bool failed_ = false;
};

} // mrdox
} // clang

#endif
