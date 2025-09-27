//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_CONFIG_REFERENCEDIRECTORIES_HPP
#define MRDOCS_API_CONFIG_REFERENCEDIRECTORIES_HPP

#include <string>


namespace mrdocs {

/** Reference directories used to resolve paths

    These are the main reference directories used to resolve paths in the
    application.

    All other reference directories come directly from the
    configuration file.
 */
struct ReferenceDirectories
{
    std::string cwd;
    std::string mrdocsRoot;
};

} // mrdocs


#endif // MRDOCS_API_CONFIG_REFERENCEDIRECTORIES_HPP
