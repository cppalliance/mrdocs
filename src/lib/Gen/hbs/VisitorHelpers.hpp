//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_HBS_VISITORHELPERS_HPP
#define MRDOCS_LIB_GEN_HBS_VISITORHELPERS_HPP

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/Info.hpp>

namespace clang::mrdocs::hbs {

/** @brief Determine if the generator should generate a page for this Info.

    This filters Info types for which the generator
    should not generate independent pages or sections.
 */
MRDOCS_DECL
bool
shouldGenerate(Info const& I, Config const& config);

/** Find an Info type whose URL we can use for the specified Info

    When we should not generate a page for the Info as per
    @ref shouldGenerate, other documentation pages might
    still link to it.

    In this case, we find a related Info type whose URL
    we can use for the specified Info.

    For specializations, we typically look for their primary
    template. For record and enum members, we look for
    the parent record or enum. For other Info types, we
    return nullptr.
 */
MRDOCS_DECL
Info const*
findAlternativeURLInfo(Corpus const& c, Info const& I);

} // clang::mrdocs::hbs

#endif
