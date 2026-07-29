//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_HELPERS_CONSTRUCTOR_HPP
#define MRDOCS_API_HANDLEBARS_HELPERS_CONSTRUCTOR_HPP

// Value-constructor helpers (str, arr, ...).

#include <mrdocs/Dom.hpp>
#include <mrdocs/Handlebars/Engine.hpp>
#include <mrdocs/Handlebars/Platform.hpp>

namespace mrdocs {
namespace handlebars {
namespace helpers {

/** Register contructor helpers into a Handlebars instance

    This function registers a number of common helpers that allows
    the user to create objects of specific types directly from
    literals in the template.

    @param hbs The Handlebars instance to register the helpers into
*/
MRDOCS_HANDLEBARS_DECL
void
registerConstructorHelpers(Handlebars& hbs);

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs

#endif
