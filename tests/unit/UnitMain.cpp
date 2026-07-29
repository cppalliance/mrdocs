//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Platform.hpp>
#include <test_suite/test_suite.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/PrettyStackTrace.h>
#include <llvm/Support/Signals.h>

// Entry point for the mrdocs unit-test suite. Golden (reference) tests live in
// a separate executable; this binary only runs the registered unit tests.
int
main(int argc, char const** argv)
{
    llvm::EnablePrettyStackTrace();
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);
    llvm::setBugReportMsg(
        "PLEASE submit a bug report to "
        "https://github.com/cppalliance/mrdocs/issues/ and include the "
        "crash backtrace.\n");

    return test_suite::unit_test_main(argc, argv);
}
