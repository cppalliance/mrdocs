//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Support/Error.hpp"
#include <mrdocs/Support/Path.hpp>
#include <llvm/Support/Mutex.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Signals.h>
#include <cstdlib>
#include <mutex>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <debugapi.h>
#include <crtdbg.h>
#include <sstream>
#endif

namespace SourceFileNames {
extern char const* getFileName(char const*) noexcept;
} // SourceFileNames

namespace clang {
namespace mrdocs {

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
    return fmt::format("{}:{}",
        ::SourceFileNames::getFileName(
            loc.file_name()),
        loc.line());
#if 0
    return fmt::format(
        "{}@{}({})",
        loc.function_name(),
        ::SourceFileNames::getFileName(
            loc.file_name()),
        loc.line());
#endif
}

std::string
Error::
formatMessage(
    std::string_view const& reason,
    std::string_view const& where)
{
    std::string result;
    result = reason;
    result.append(" (");
    result.append(where);
    result.append(")");
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
    MRDOCS_ASSERT(! message_.empty());
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
    std::exception const& ex)
{
    std::string_view s(ex.what());
    if(s.empty())
        s = "unknown exception";
    reason_ = s;
    message_ = s;
}

Error::
Error(
    std::vector<Error> const& errors,
    source_location loc)
{
    MRDOCS_ASSERT(errors.size() > 0);
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
    MRDOCS_ASSERT(failed());
    throw Exception(*this);
}

void Error::Throw() &&
{
    MRDOCS_ASSERT(failed());
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

namespace report {

static llvm::sys::Mutex mutex_;
static Level level_ = Level::debug;

constinit Results results{};

static
void
print_impl(
    std::string const& s)
{
    llvm::errs() << s << '\n';
#ifdef _MSC_VER
    if(::IsDebuggerPresent() != 0)
    {
        ::OutputDebugStringA(s.c_str());
        ::OutputDebugStringA("\n");
    }
#endif
}

void
setMinimumLevel(
    Level level) noexcept
{
    level_ = level;
}

void
print(
    std::string const& text)
{
    print_impl(text);
}

void
print(
    Level level,
    std::string const& text,
    source_location const* loc)
{
    call_impl(level,
        [&](llvm::raw_ostream& os)
        {
            os << text;
        }, loc);
}

//------------------------------------------------

Level
getLevel(unsigned level) noexcept
{
    switch(level)
    {
    case 0: return Level::debug;
    case 1: return Level::info;
    case 2: return Level::warn;
    case 3: return Level::error;
    default:
        return Level::fatal;
    }
}

void
call_impl(
    Level level,
    std::function<void(llvm::raw_ostream&)> f,
    source_location const* loc)
{
    std::string s;
    if(level >= level_)
    {
        llvm::raw_string_ostream os(s);
        f(os);
        if(loc && (
            level == Level::warn ||
            level == Level::error ||
            level == Level::fatal))
        {
            os << "\n" <<
                fmt::format(
                    "    Reported at {}({})",
                    ::SourceFileNames::getFileName(loc->file_name()),
                    loc->line());
            // VFALCO attach a stack trace for Level::fatal
        }
        os << '\n';
    }
    std::lock_guard<llvm::sys::Mutex> lock(mutex_);
    if(! s.empty())
        llvm::errs() << s;
    switch(level)
    {
    case Level::debug:
        ++results.debugCount;
        break;
    case Level::info:
        ++results.infoCount;
        break;
    case Level::warn:
        ++results.warnCount;
        break;
    case Level::error:
        ++results.errorCount;
        break;
    case Level::fatal:
        ++results.fatalCount;
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
}

} // report

} // mrdocs
} // clang
