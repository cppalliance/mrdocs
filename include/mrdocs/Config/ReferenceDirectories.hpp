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

#ifndef MRDOCS_API_CONFIG_REFERENCE_DIRECTORIES_HPP
#define MRDOCS_API_CONFIG_REFERENCE_DIRECTORIES_HPP

#include <string>

namespace clang {
namespace mrdocs {

/** Reference directories used to resolve paths
 */
struct ReferenceDirectories {
    std::string configDir;
    std::string cwd;
    std::string mrdocsRoot;
};

} // mrdocs
} // clang

#endif
