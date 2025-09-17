//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SINGLEFILEDB_HPP
#define MRDOCS_LIB_SINGLEFILEDB_HPP

#include <mrdocs/Support/Path.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include "ConfigImpl.hpp"
#include <string>
#include <utility>
#include <vector>

namespace clang {
namespace mrdocs {

/** A compilation database generated from the mrdocs.yml options
*/
class MrDocsSettingsDB
    : public tooling::CompilationDatabase
{
    std::vector<tooling::CompileCommand> cc_;

public:
    explicit
    MrDocsSettingsDB(
        ConfigImpl const& config);

    std::vector<tooling::CompileCommand>
    getCompileCommands(
        llvm::StringRef FilePath) const override;

    std::vector<std::string>
    getAllFiles() const override;

    std::vector<tooling::CompileCommand>
    getAllCompileCommands() const override;
};

} // mrdocs
} // clang

#endif
