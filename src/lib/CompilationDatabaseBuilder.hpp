//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_COMPILATIONDATABASEBUILDER_HPP
#define MRDOCS_LIB_COMPILATIONDATABASEBUILDER_HPP

#include <lib/ConfigImpl.hpp>
#include <lib/MrDocsCompilationDatabase.hpp>


namespace mrdocs {

/** Build the compilation database described by the configuration.
*/
Expected<MrDocsCompilationDatabase>
generateCompilationDatabase(
    std::shared_ptr<ConfigImpl const> const& config);

} // mrdocs


#endif // MRDOCS_LIB_COMPILATIONDATABASEBUILDER_HPP
