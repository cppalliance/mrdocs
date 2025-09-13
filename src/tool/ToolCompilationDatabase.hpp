//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TOOL_COMPILATIONDATABASE_HPP
#define MRDOCS_TOOL_COMPILATIONDATABASE_HPP

#include "lib/MrDocsCompilationDatabase.hpp"
#include "lib/ConfigImpl.hpp"

namespace clang {
namespace mrdocs {

Expected<MrDocsCompilationDatabase>
generateCompilationDatabase(
    std::string_view tempDir,
    std::shared_ptr<ConfigImpl const> const& config,
    ThreadPool& threadPool);

} // mrdocs
} // clang

#endif
