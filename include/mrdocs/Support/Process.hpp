//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_PROCESS_HPP
#define MRDOCS_API_SUPPORT_PROCESS_HPP

#include <mrdocs/Platform.hpp>
#include <iosfwd>
#include <string>
#include <string_view>

namespace mrdocs {

/** Install the handlers that print a stack trace when the process crashes.

    A program that links MrDocs calls this once, at the start of `main`,
    instead of using the LLVM support library directly. The program then
    needs no LLVM of its own, and the crash state lives in one copy. Call
    it once: later calls keep the values of the first.

    @param argv0 The first element of `argv`, which the handlers use to find
    the symbols of the executable.

    @param bugReportMessage The text printed before the trace, which tells the
    user where to report the crash.
*/
MRDOCS_DECL
void
installCrashHandlers(
    std::string_view argv0,
    std::string_view bugReportMessage);

/** Write the stack trace of the calling thread to a stream.

    @param os The stream to write to.
*/
MRDOCS_DECL
void
printStackTrace(std::ostream& os);

/** Return the target triple LLVM was configured to compile for by default.

    That is the triple of the host, unless LLVM was configured otherwise. The
    triple has the form `arm64-apple-darwin25.0.0`.
*/
MRDOCS_DECL
std::string
defaultTargetTriple();

/** Return the version of LLVM that MrDocs was built with.

    The version has the form `23.0.0git`.
*/
MRDOCS_DECL
std::string_view
llvmVersion() noexcept;

/** Return the path of the running executable.

    @param argv0 The first element of `argv`, used when the system cannot
    tell where the executable is.

    @param mainAddress The address of a function in the executable, used the
    same way.

    @return The absolute path, or an empty string if it cannot be found.
*/
MRDOCS_DECL
std::string
executablePath(
    std::string_view argv0,
    void* mainAddress);

} // mrdocs

#endif
