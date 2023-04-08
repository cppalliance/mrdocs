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

#ifndef MRDOX_ERRORS_HPP
#define MRDOX_ERRORS_HPP

#include <mrdox/detail/Errors.hpp>
#include <llvm/Support/Error.h>
#include <llvm/Support/raw_ostream.h>
#include <cassert>
#include <new>
#include <source_location>
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
    int
    getExitCode() const noexcept
    {
        if(! failed_)
            return EXIT_SUCCESS;
        return EXIT_FAILURE;
    }

    bool
    failed(
        llvm::StringRef what,
        std::error_code const& ec,
        std::source_location const& loc =
            std::source_location::current())
    {
        if(! ec)
            return false;
        print(what, llvm::createStringError(
            ec, ec.message()), loc);
        return true;
    }

    bool
    failed(
        llvm::StringRef what,
        llvm::Error&& err,
        std::source_location const& loc =
            std::source_location::current())
    {
        if(! err)
            return false;
        print(what, std::move(err), loc);
        return true;
    }

    bool
    failed(
        llvm::StringRef what,
        llvm::Error& err,
        std::source_location const& loc =
            std::source_location::current())
    {
        return failed(what, std::move(err), loc);
    }

    template<class T>
    bool
    failed(
        llvm::StringRef what,
        llvm::Expected<T>&& ex,
        std::source_location const& loc =
            std::source_location::current())
    {
        if(ex)
            return false;
        print(what, ex.takeError(), loc);
        return true;
    }

    template<class T>
    bool
    failed(
        llvm::StringRef what,
        llvm::Expected<T>& ex,
        std::source_location const& loc =
            std::source_location::current())
    {
        return failed(what, std::move(ex), loc);
    }


    template<class T>
    bool
    failed(
        llvm::StringRef what,
        llvm::ErrorOr<T>&& eor,
        std::source_location const& loc =
            std::source_location::current())
    {
        return failed(what, eor.getError(), loc);
    }

    template<class T>
    bool
    failed(
        llvm::StringRef what,
        llvm::ErrorOr<T>& eor,
        std::source_location const& loc =
            std::source_location::current())
    {
        return failed(what, eor.getError(), loc);
    }

    void testFailed()
    {
        failed_ = true;
    }

private:
    void print(llvm::StringRef what, llvm::Error&& err,
        std::source_location const& loc);

    bool failed_ = false;
};

} // mrdox
} // clang

#endif
