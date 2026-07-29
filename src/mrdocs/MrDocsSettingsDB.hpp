//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_MRDOCSSETTINGSDB_HPP
#define MRDOCS_LIB_MRDOCSSETTINGSDB_HPP

#include <mrdocs/Config.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include <string>
#include <utility>
#include <vector>


namespace mrdocs {

/** A compilation database generated from the mrdocs.yml options
*/
class MrDocsSettingsDB
    : public clang::tooling::CompilationDatabase
{
    std::vector<clang::tooling::CompileCommand> cc_;

public:
    explicit
    MrDocsSettingsDB(
        Config const& config);

    std::vector<clang::tooling::CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override;

    std::vector<std::string>
    getAllFiles() const override;

    std::vector<clang::tooling::CompileCommand>
    getAllCompileCommands() const override;
};

} // mrdocs


#endif
