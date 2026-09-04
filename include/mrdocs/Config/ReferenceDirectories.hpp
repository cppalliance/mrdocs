//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_CONFIG_REFERENCEDIRECTORIES_HPP
#define MRDOCS_API_CONFIG_REFERENCEDIRECTORIES_HPP

#include <mrdocs/Platform.hpp>
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
    /** Absolute path to the current working directory.
    */
    std::string cwd;
    /** Absolute path to the MrDocs repository/install root.

        All of MrDocs's built-in directories (addons and the parse-time header
        sets) derive from this: `<mrdocsRoot>/share/mrdocs/...`.
    */
    std::string mrdocsRoot;

    /** Construct and resolve the reference directories.

        The default root is the compile-time MRDOCS_DEFAULT_ROOT, which
        mrdocs-config.cmake injects for a downstream project. It is a default
        argument, so it is evaluated in the caller's translation unit -- that is
        what lets a linking project's find_package-provided prefix reach here.
        The constructor then refines it: MRDOCS_ROOT in the environment wins;
        otherwise an empty root falls back to the running executable's location.
        Defined in mrdocs-core to keep the platform/LLVM lookups out of this
        header.
    */
    MRDOCS_DECL explicit ReferenceDirectories(std::string root = MRDOCS_DEFAULT_ROOT);
};

} // mrdocs


#endif // MRDOCS_API_CONFIG_REFERENCEDIRECTORIES_HPP
