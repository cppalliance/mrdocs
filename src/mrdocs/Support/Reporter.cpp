//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Platform.hpp>
#include "ReporterImpl.hpp"
#include <mrdocs/Support/Error/Assert.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Version.hpp>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/Signals.h>
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


namespace mrdocs {

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

namespace {

// The console is one for the whole process, so the reporters of every run
// write to it under one lock. The lock holds no setting and no count.
std::mutex&
consoleMutex() noexcept
{
    static std::mutex m;
    return m;
}

} // (anon)

Reporter::
Reporter() noexcept
    : Reporter(Level::debug)
{
}

Reporter::
Reporter(Level const minimumLevel) noexcept
    : level_(minimumLevel)
{
}

Reporter::
~Reporter() = default;

void
Reporter::
setMinimumLevel(Level const level) noexcept
{
    level_ = level;
}

Level
Reporter::
getMinimumLevel() const noexcept
{
    return level_;
}

void
Reporter::
setSourceLocationWarnings(bool const b) noexcept
{
    sourceLocationWarnings_ = b;
}

void
Reporter::
setWarnAsError(bool const b) noexcept
{
    warnAsError_ = b;
}

bool
Reporter::
warnAsError() const noexcept
{
    return warnAsError_;
}

Results
Reporter::
results() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return results_;
}

void
Reporter::
print(
    std::string const& s)
{
    std::lock_guard<std::mutex> lock(consoleMutex());
    llvm::outs() << s << '\n';
#ifdef _MSC_VER
    if(::IsDebuggerPresent() != 0)
    {
        ::OutputDebugStringA(s.c_str());
        ::OutputDebugStringA("\n");
    }
#endif
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

namespace {

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

} // (anon)

void
Reporter::
print(
    Level const level,
    std::string const& text,
    source_location const* loc,
    Error const* e)
{
    std::string s;
    if(level >= level_)
    {
        llvm::raw_string_ostream os(s);
        os << text;
        using LT = std::underlying_type_t<Level>;
        if(sourceLocationWarnings_ &&
           loc &&
           static_cast<LT>(level) >= static_cast<LT>(Level::error))
        {
            os << "\n\n";
            os << "An issue occurred during execution.\n";
            os << "If you believe this is a bug, please report it at https://github.com/cppalliance/mrdocs/issues\n"
                  "with the following details:\n";
            os << std::format("    MrDocs Release: {}\n",
                              project_release_with_build.empty()
                                  ? std::string_view("unknown")
                                  : project_release_with_build);
            os << std::format("    MrDocs Version: {} (Build: {})\n",
                              project_version, project_version_build);
            if (e)
            {
              os << std::format(
                  "    Error Location: `{}` at line {}\n",
                  files::makeProjectRelative(e->location().file_name()),
                  e->location().line());
            }
            os << std::format("    Reported From: `{}` at line {}\n",
                              files::makeProjectRelative(loc->file_name()),
                              loc->line());
            // VFALCO attach a stack trace for Level::fatal
        }
        os << '\n';
    }

    if (!s.empty())
    {
        std::lock_guard<std::mutex> lock(consoleMutex());
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

    // Update counters
    std::lock_guard<std::mutex> lock(mutex_);
    switch(level)
    {
    case Level::trace:
        ++results_.traceCount;
        break;
    case Level::debug:
        ++results_.debugCount;
        break;
    case Level::info:
        ++results_.infoCount;
        break;
    case Level::warn:
        ++results_.warnCount;
        break;
    case Level::error:
        ++results_.errorCount;
        break;
    case Level::fatal:
        ++results_.fatalCount;
        break;
    default:
        MRDOCS_UNREACHABLE();
    }
}

} // report

} // mrdocs

