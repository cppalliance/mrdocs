//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Support/Error.hpp"
#include <mrdox/Support/Path.hpp>
#include <llvm/Support/Mutex.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Signals.h>
#include <cstdlib>
#include <mutex>

namespace SourceFileNames {
extern char const* getFileName(char const*) noexcept;
} // SourceFileNames

namespace clang {
namespace mrdox {

//------------------------------------------------
//
// Error
//
//------------------------------------------------

std::string
Error::
formatWhere(
    source_location const& loc)
{
    return fmt::format(
        "{}@{}({})",
        loc.function_name(),
        ::SourceFileNames::getFileName(
            loc.file_name()),
        loc.line());
}

std::string
Error::
formatMessage(
    std::string_view const& reason,
    std::string_view const& where)
{
    std::string result;
    result = reason;
    result.append(": ");
    result.append(where);
    return result;
}

Error::
Error(
    std::string reason,
    source_location loc)
    : where_(formatWhere(loc))
    , reason_(std::move(reason))
    , message_(formatMessage(reason_, where_))
    , loc_(loc)
{
    MRDOX_ASSERT(! message_.empty());
}

Error::
Error(
    std::error_code const& ec,
    source_location loc)
{
    if(! ec)
        return;
    where_ = formatWhere(loc);
    reason_ = ec.message();
    message_ = formatMessage(reason_, where_);
    loc_ = loc;
}

Error::
Error(
    std::vector<Error> const& errors,
    source_location loc)
{
    MRDOX_ASSERT(errors.size() > 0);
    if(errors.size() == 1)
    {
        *this = errors.front();
        return;
    }

    where_ = formatWhere(loc);
    reason_ = fmt::format(
        "{} errors occurred:\n", errors.size());
    for(auto const& err : errors)
    {
        reason_.append("    ");
        reason_.append(err.message());
        reason_.push_back('\n');
    }
    message_ = formatMessage(reason_, where_);
    loc_ = loc;
}

void Error::Throw() const&
{
    MRDOX_ASSERT(failed());
    throw Exception(*this);
}

void Error::Throw() &&
{
    MRDOX_ASSERT(failed());
    throw Exception(std::move(*this));
}

//------------------------------------------------

SourceLocation::
SourceLocation(
    source_location const& loc) noexcept
    : file_(files::getSourceFilename(loc.file_name()))
    , line_(loc.line())
    , col_(loc.column())
    , func_(loc.function_name())
{
}

//------------------------------------------------
//
// Reporting
//
//------------------------------------------------

static llvm::sys::Mutex reportMutex_;

namespace report {

// minimum level to print
static unsigned reportLevel_ = 0;

constinit Results results{};

void setMinimumLevel(unsigned level) noexcept
{
    if( level > 4)
        level = 4;
    reportLevel_ = level;
}

void
print_impl(
    unsigned level,
    std::string_view text,
    source_location const* loc)
{
    call_impl(level,
        [&](llvm::raw_ostream& os)
        {
            os << text;
        }, loc);
}

void
call_impl(
    unsigned level,
    std::function<void(llvm::raw_ostream&)> f,
    source_location const* loc)
{
    MRDOX_ASSERT(level <= 4);
    if( level > 4)
        level = 4;
    std::lock_guard<llvm::sys::Mutex> lock(reportMutex_);
    if(level >= reportLevel_)
    {
        f(llvm::errs());
        if(loc)
            llvm::errs() << "\n" <<
                fmt::format(
                    "    Reported at {}({})",
                    ::SourceFileNames::getFileName(loc->file_name()),
                    loc->line());
        llvm::errs() << '\n';
    }
    switch(level)
    {
    case 0:
        ++results.debugCount;
        break;
    case 1:
        ++results.infoCount;
        break;
    case 2:
        ++results.warnCount;
        break;
    case 3:
        ++results.errorCount;
        break;
    case 4:
        ++results.fatalCount;
        break;
    default:
        MRDOX_UNREACHABLE();
    }
}

} // report

//------------------------------------------------

void
reportError(
    std::string_view text)
{
    std::lock_guard<llvm::sys::Mutex> lock(reportMutex_);
    llvm::errs() << text << '\n';
}

void
reportWarning(
    std::string_view text)
{
    std::lock_guard<llvm::sys::Mutex> lock(reportMutex_);
    llvm::errs() << text << '\n';
}

void
reportInfo(
    std::string_view text)
{
    std::lock_guard<llvm::sys::Mutex> lock(reportMutex_);
    llvm::errs() << text << '\n';
}

void
reportUnhandledException(
    std::exception const& ex)
{
    namespace sys = llvm::sys;

    std::lock_guard<llvm::sys::Mutex> lock(reportMutex_);
    llvm::errs() <<
        "Unhandled exception: " << ex.what() << '\n';
    sys::PrintStackTrace(llvm::errs());
    std::exit(EXIT_FAILURE);
}

} // mrdox
} // clang
