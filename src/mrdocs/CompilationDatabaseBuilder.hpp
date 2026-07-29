//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_COMPILATIONDATABASEBUILDER_HPP
#define MRDOCS_LIB_COMPILATIONDATABASEBUILDER_HPP

#include "MrDocsCompilationDatabase.hpp"
#include <mrdocs/Config.hpp>


namespace mrdocs {

/** Build the compilation database described by the configuration.
*/
Expected<MrDocsCompilationDatabase>
generateCompilationDatabase(
    Config const& config);

} // mrdocs


#endif // MRDOCS_LIB_COMPILATIONDATABASEBUILDER_HPP
