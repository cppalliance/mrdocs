//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_HELPERS_CONTAINER_HPP
#define MRDOCS_API_HANDLEBARS_HELPERS_CONTAINER_HPP

// Object/array (container) helpers.

#include <mrdocs/Dom.hpp>
#include <mrdocs/Handlebars/Engine.hpp>
#include <mrdocs/Handlebars/Platform.hpp>

namespace mrdocs {
namespace handlebars {
namespace helpers {

/** Register helpers to manipulate composite data types

    This function registers a number of common helpers that operate on
    Objects and Arrays. Object and Array helpers are particularly useful
    because most applications will need to manipulate Objects and Arrays
    to extract information from them, such as object keys or specific
    Array items known ahead of time.

    The helper names are inspired by the default functions provided
    in multiple programming languages for dictionaries, objects, and arrays,
    such as Python and JavaScript, for their default types.

    The individual helpers are defined as an implementation detail and
    cannot be registered individually.

    @param hbs The Handlebars instance to register the helpers into
*/
MRDOCS_HANDLEBARS_DECL
void
registerContainerHelpers(Handlebars& hbs);

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs

#endif
