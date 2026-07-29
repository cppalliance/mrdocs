//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_HELPERS_MATH_HPP
#define MRDOCS_API_HANDLEBARS_HELPERS_MATH_HPP

// Math and type helpers.

#include <mrdocs/Dom.hpp>
#include <mrdocs/Handlebars/Engine.hpp>
#include <mrdocs/Handlebars/Platform.hpp>

namespace mrdocs {
namespace handlebars {
namespace helpers {

/** Register math helpers into a Handlebars instance

    This function registers a number of common helpers that perform
    mathemathical operations.

    @param hbs The Handlebars instance to register the helpers into
*/
MRDOCS_HANDLEBARS_DECL
void
registerMathHelpers(Handlebars& hbs);

/** Register type helpers into a Handlebars instance

    This function registers a number of common helpers that operate on
    types, such as identity, type checking, and type conversion.

    @param hbs The Handlebars instance to register the helpers into
*/
MRDOCS_HANDLEBARS_DECL
void
registerTypeHelpers(Handlebars& hbs);

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs

#endif
