//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_ADDONROOTS_HPP
#define MRDOCS_LIB_SUPPORT_ADDONROOTS_HPP

#include <mrdocs/Config.hpp>
#include <mrdocs/Support/Path.hpp>

#include <string>
#include <vector>

namespace mrdocs {

/** Return the existing addon roots in load order.

    Primary `addons` first, then each entry of `addons-supplemental`
    in the order it appears. Missing paths are skipped silently.

    Used by both the Handlebars generator (to locate templates,
    partials, and helpers) and the extension stack (to locate
    corpus-mutation scripts).
*/
inline std::vector<std::string>
addonRoots(Config const& config)
{
    std::vector<std::string> roots;
    roots.reserve(1 + config->addonsSupplemental.size());
    if (files::exists(config->addons))
    {
        roots.push_back(config->addons);
    }
    for (std::string const& supplemental : config->addonsSupplemental)
    {
        if (files::exists(supplemental))
        {
            roots.push_back(supplemental);
        }
    }
    return roots;
}

} // mrdocs

#endif
