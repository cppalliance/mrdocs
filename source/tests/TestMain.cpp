//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Tester.hpp"
#include <clang/Tooling/AllTUsExecution.h>
#include <llvm/Support/Signals.h>

namespace clang {
namespace mrdox {

void
testMain(
    int argc, const char* const* argv,
    Reporter& R)
{
    namespace path = llvm::sys::path;

    // Each command line argument is processed
    // as a directory which will be iterated
    // recursively for tests.
    for(int i = 1; i < argc; ++i)
    {
        auto config = Config::createAtDirectory(argv[i]);
        if(! config)
            return (void)R.error(config, "create config at directory '", argv[i], "'");

        // Set source root to config dir
        if(auto err = (*config)->setSourceRoot((*config)->configDir()))
            return (void)R.error(err, "set source root to '", (*config)->configDir(), "'");

        (*config)->setVerbose(false);

        // We need a different config for each directory
        // passed on the command line, and thus each must
        // also have a separate Tester.
        Tester tester(**config, R);
        llvm::StringRef s(argv[i]);
        llvm::SmallString<340> dirPath(s);
        path::remove_dots(dirPath, true);
        llvm::ThreadPool threadPool(
            llvm::hardware_concurrency(
                tooling::ExecutorConcurrency));
        if(! tester.checkDirRecursively(
                llvm::StringRef(argv[i]), threadPool))
            break;
        threadPool.wait();
    }
}

} // mrdox
} // clang

int
main(int argc, const char** argv)
{
    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

    clang::mrdox::Reporter R;
    clang::mrdox::testMain(argc, argv, R);
    return R.getExitCode();
}
