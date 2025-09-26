//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Platform.hpp>
#include <lib/Support/Report.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Version.hpp>
#include <llvm/Support/Mutex.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/raw_ostream.h>
#include <cstdlib>
#include <format>
#include <mutex>

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
// clang-format off
#include <windows.h>
#include <debugapi.h>
#include <crtdbg.h>
#include <sstream>
// clang-format on
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
  return std::format("{}:{}", ::SourceFileNames::getFileName(loc.file_name()),
                     loc.line());
#if 0
    return std::format(
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
    reason_ = std::format("{} errors occurred:\n", errors.size());
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
static bool sourceLocationWarnings_ = true;

constinit Results results{};

void
setMinimumLevel(Level const level) noexcept
{
    level_ = level;
}

Level
getMinimumLevel() noexcept
{
    return level_;
}

void
setSourceLocationWarnings(bool b) noexcept
{
    sourceLocationWarnings_ = b;
}

void
print(
    std::string const& s)
{
    llvm::outs() << s << '\n';
#ifdef _MSC_VER
    if(::IsDebuggerPresent() != 0)
    {
        ::OutputDebugStringA(s.c_str());
        ::OutputDebugStringA("\n");
    }
#endif
}

void
print(
    Level level,
    std::string const& text,
    source_location const* loc,
    Error const* e)
{
    call_impl(level,
        [&](llvm::raw_ostream& os)
        {
            os << text;
        }, loc, e);
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

constexpr
llvm::raw_ostream::Colors
getLevelColor(Level level)
{
    switch(level)
    {
    case Level::trace:
        return llvm::raw_ostream::Colors::CYAN;
    case Level::debug:
        return llvm::raw_ostream::Colors::GREEN;
    case Level::info:
        return llvm::raw_ostream::Colors::WHITE;
    case Level::warn:
        return llvm::raw_ostream::Colors::BRIGHT_YELLOW;
    case Level::error:
        return llvm::raw_ostream::Colors::RED;
    case Level::fatal:
        return llvm::raw_ostream::Colors::RED;
    default:
        MRDOCS_UNREACHABLE();
    }
}


void
call_impl(
    Level level,
    std::function<void(llvm::raw_ostream&)> f,
    source_location const* loc,
    Error const* e)
{
    std::string s;
    if(level >= level_)
    {
        llvm::raw_string_ostream os(s);
        f(os);
        using LT = std::underlying_type_t<Level>;
        if(sourceLocationWarnings_ &&
           loc &&
           static_cast<LT>(level) >= static_cast<LT>(Level::error))
        {
            os << "\n\n";
            os << "An issue occurred during execution.\n";
            os << "If you believe this is a bug, please report it at https://github.com/cppalliance/mrdocs/issues\n"
                  "with the following details:\n";
            os << std::format("    MrDocs Version: {} (Build: {})\n",
                              project_version, project_version_build);
            if (e)
            {
              os << std::format(
                  "    Error Location: `{}` at line {}\n",
                  ::SourceFileNames::getFileName(e->location().file_name()),
                  e->location().line());
            }
            os << std::format("    Reported From: `{}` at line {}\n",
                              ::SourceFileNames::getFileName(loc->file_name()),
                              loc->line());
            // VFALCO attach a stack trace for Level::fatal
        }
        os << '\n';
    }

    // Update counters
    std::lock_guard<llvm::sys::Mutex> lock(mutex_);
    if (!s.empty())
    {
        if (level >= Level::error)
        {
            llvm::errs() << s;
        }
        else
        {
            llvm::outs() << getLevelColor(level) << s << llvm::raw_ostream::Colors::RESET;
            llvm::outs().flush();
        }
    }
    switch(level)
    {
    case Level::trace:
        ++results.traceCount;
        break;
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
