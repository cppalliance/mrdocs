//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TESTS_TESTFILES_HPP
#define MRDOX_TESTS_TESTFILES_HPP

#include <mrdox/Errors.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/ADT/StringRef.h>

namespace clang {
namespace mrdox {

/** Compilation database where files come in pairs of C++ and XML.
*/
class TestFiles
    : public tooling::CompilationDatabase
{
    std::vector<tooling::CompileCommand> cc_;

public:
    bool
    addDirectory(
        llvm::StringRef path,
        Reporter& R);

    std::vector<tooling::CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override;

    std::vector<std::string>
    getAllFiles() const override;

    std::vector<tooling::CompileCommand>
    getAllCompileCommands() const override;
};

} // mrdox
} // clang

#endif
