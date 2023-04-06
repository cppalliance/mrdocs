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

#include "BitcodeReader.h"
#include "BitcodeWriter.h"
#include "Generators.h"
#include "ClangDoc.h"
#include <mrdox/Config.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <llvm/Support/Mutex.h>
#include <llvm/Support/ThreadPool.h>

namespace clang {
namespace mrdox {


//------------------------------------------------

Config::
Config()
{
    llvm::SmallString<128> SourceRootDir;
    llvm::sys::fs::current_path(SourceRootDir);
    SourceRoot = std::string(SourceRootDir.str());
}

} // mrdox
} // clang
