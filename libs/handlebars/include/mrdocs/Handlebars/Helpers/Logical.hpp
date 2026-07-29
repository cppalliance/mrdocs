//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_HELPERS_LOGICAL_HPP
#define MRDOCS_API_HANDLEBARS_HELPERS_LOGICAL_HPP

// Logical/comparison helpers and the public and_fn/or_fn/eq_fn/... functions.

#include <mrdocs/Dom.hpp>
#include <mrdocs/Handlebars/Engine.hpp>
#include <mrdocs/Handlebars/Platform.hpp>

namespace mrdocs {
namespace handlebars {
namespace helpers {

/** Register logical helpers into a Handlebars instance

    This function registers a number of common helpers that perform
    logical operations.

    @param hbs The Handlebars instance to register the helpers into
*/
MRDOCS_HANDLEBARS_DECL
void
registerLogicalHelpers(Handlebars& hbs);

/** "and" helper function

    The "and" helper returns true if all of the values are truthy.

    @param args The values to test
    @return True if all of the values are truthy, false otherwise.
*/
MRDOCS_HANDLEBARS_DECL
bool
and_fn(dom::Array const& args);

/** "or" helper function
 *
 *  The "or" helper returns true if any of the values are truthy.
*/
MRDOCS_HANDLEBARS_DECL
dom::Value
or_fn(dom::Array const& args);

/** "eq" helper function

    The "eq" helper returns true if all of the values are equal.

    @param args The values to compare
    @return True if all of the values are equal, false otherwise.
*/
MRDOCS_HANDLEBARS_DECL
bool
eq_fn(dom::Array const& args);

/** "ne" helper function

    The "ne" helper returns true if any of the values are not equal.

    @param args The values to compare
    @return True if any of the values are not equal, false otherwise.
*/
MRDOCS_HANDLEBARS_DECL
bool
ne_fn(dom::Array const& args);

/** "gt" helper function

    The "gt" helper returns true if the first argument compares
    greater than the second, via @ref dom::Value's `operator<=>`.

    @param args The two values to compare.
    @return True if the first value is greater than the second.
*/
MRDOCS_HANDLEBARS_DECL
bool
gt_fn(dom::Array const& args);

/** "not" helper function

    The "not" helper returns true if not all of the values are truthy.

    @return True if not all of the values are truthy, false otherwise.
*/
MRDOCS_HANDLEBARS_DECL
bool
not_fn(dom::Array const& arg);

/** "select" helper function
 *
 *  The "select" helper returns the second argument if the first argument is
 *  truthy, and the third argument otherwise.
*/
MRDOCS_HANDLEBARS_DECL
dom::Value
select_fn(
    dom::Value const& condition,
    dom::Value const& result_true,
    dom::Value const& result_false);

/** "increment" helper function
 *
 *  The "increment" helper adds 1 to the value if it's an integer and converts
 *  booleans to `true`. Other values are returned as-is.
*/
MRDOCS_HANDLEBARS_DECL
dom::Value
increment_fn(dom::Value const& value);

/** "detag" helper function
 *
 *  The "detag" helper applies the regex expression "<[^>]+>" to the
 *  input to remove all HTML tags.
*/
MRDOCS_HANDLEBARS_DECL
dom::Value
detag_fn(dom::Value html);

/** "year" helper function

    The "year" helper returns the current year as an integer.

    @return The current year as an integer.
*/
MRDOCS_HANDLEBARS_DECL
int
year_fn();

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs

#endif
