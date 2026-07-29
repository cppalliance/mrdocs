//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_HELPERS_STRING_HPP
#define MRDOCS_API_HANDLEBARS_HELPERS_STRING_HPP

// String manipulation helpers.

#include <mrdocs/Dom.hpp>
#include <mrdocs/Handlebars/Engine.hpp>
#include <mrdocs/Handlebars/Platform.hpp>

namespace mrdocs {
namespace handlebars {
namespace helpers {

/** Register string helpers into a Handlebars instance

    This function registers a number of common helpers that operate on
    strings. String helpers are particularly useful because most
    applications will need to manipulate strings for presentation
    purposes.

    All helpers can be used as either block helpers or inline helpers.
    When used as a block helper, the block content is used as the first
    argument to the helper function. When used as an inline helper, the
    first argument is the value of the helper.

    The helper names are inspired by the default string functions provided
    in multiple programming languages, such as Python and JavaScript,
    for their default string types.

    The individual helpers are defined as an implementation detail and
    cannot be registered individually.

    @param hbs The Handlebars instance to register the helpers into
*/
MRDOCS_HANDLEBARS_DECL
void
registerStringHelpers(Handlebars& hbs);

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs

#endif
