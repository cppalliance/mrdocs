//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_EXTENSIONS_ADDONDISCOVERY_HPP
#define MRDOCS_LIB_EXTENSIONS_ADDONDISCOVERY_HPP

#include <mrdocs/Config.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <string>
#include <vector>

namespace mrdocs {

/** Return the extension entry scripts across every addon root.

    Walks the immediate children of the extensions/ directory under each
    addon root: a .lua or .js file is an extension. The result is sorted
    alphabetically by full path, interleaving the two languages so
    behavior depends only on file names.
*/
Expected<std::vector<std::string>>
collectExtensionScripts(Config const& config);

} // mrdocs

#endif
