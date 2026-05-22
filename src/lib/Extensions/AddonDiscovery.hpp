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
#include <mrdocs/Support/Expected.hpp>
#include <string>
#include <vector>

namespace mrdocs {

/** Return the extension scripts across every addon root.

    Walks `<root>/extensions/` under each addon root and gathers every
    `.lua` and `.js` file, sorted alphabetically by full path. The two
    languages are interleaved so behavior doesn't depend on which
    language a user happens to write in - only on file names.
*/
Expected<std::vector<std::string>>
collectExtensionScripts(Config const& config);

} // mrdocs

#endif
