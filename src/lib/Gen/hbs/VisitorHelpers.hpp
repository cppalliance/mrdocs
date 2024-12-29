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

#include <mrdocs/Metadata/Info.hpp>

namespace clang::mrdocs::hbs {

/** @brief Determine if the generator should generate a page for this Info.

    This filters Info types for which the generator
    should not generate independent pages or sections.
 */
MRDOCS_DECL
bool
shouldGenerate(Info const& I);

} // clang::mrdocs::hbs

#endif
