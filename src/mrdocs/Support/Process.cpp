//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "RawOstream.hpp"
#include <mrdocs/Support/Process.hpp>
#include <llvm/Config/llvm-config.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>
#include <llvm/TargetParser/Host.h>
#include <ostream>

namespace mrdocs {

void
installCrashHandlers(
    std::string_view argv0,
    std::string_view bugReportMessage)
{
    // LLVM keeps the pointers it is given, so both texts have to outlive the
    // process.
    static std::string const argv0Storage(argv0);
    static std::string const messageStorage(bugReportMessage);
    llvm::EnablePrettyStackTrace();
    llvm::sys::PrintStackTraceOnErrorSignal(argv0Storage);
    llvm::setBugReportMsg(messageStorage.c_str());
}

void
printStackTrace(std::ostream& os)
{
    RawOstream raw(os);
    llvm::sys::PrintStackTrace(raw);
}

std::string
defaultTargetTriple()
{
    return llvm::sys::getDefaultTargetTriple();
}

std::string_view
llvmVersion() noexcept
{
    return LLVM_VERSION_STRING;
}

std::string
executablePath(
    std::string_view argv0,
    void* mainAddress)
{
    std::string const argv0Text(argv0);
    return llvm::sys::fs::getMainExecutable(argv0Text.c_str(), mainAddress);
}

} // mrdocs
