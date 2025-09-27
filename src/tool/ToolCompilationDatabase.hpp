//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_TOOL_TOOLCOMPILATIONDATABASE_HPP
#define MRDOCS_TOOL_TOOLCOMPILATIONDATABASE_HPP

#include <lib/ConfigImpl.hpp>
#include <lib/MrDocsCompilationDatabase.hpp>


namespace mrdocs {

Expected<MrDocsCompilationDatabase>
generateCompilationDatabase(
    std::string_view tempDir,
    std::shared_ptr<ConfigImpl const> const& config);

} // mrdocs


#endif // MRDOCS_TOOL_TOOLCOMPILATIONDATABASE_HPP
