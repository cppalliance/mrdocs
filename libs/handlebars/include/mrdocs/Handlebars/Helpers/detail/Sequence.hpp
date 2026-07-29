//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_HELPERS_DETAIL_SEQUENCE_HPP
#define MRDOCS_API_HANDLEBARS_HELPERS_DETAIL_SEQUENCE_HPP

// Internal sequence/range operations on dom values, shared across the helper
// categories that manipulate arrays and strings (container, string, logical).
// Not part of the public API.

#include <mrdocs/Dom.hpp>
#include <cstdint>

namespace mrdocs {
namespace handlebars {
namespace detail {

/** Normalize a possibly-negative index into the range [0, n). */
constexpr
std::int64_t
normalize_index(std::int64_t i, std::int64_t n)
{
    if (i < 0 || i > n)
    {
        return (i % n + n) % n;
    }
    return i;
}

/** Return the element of a range at a field/index. */
dom::Value
at_fn(dom::Value range, dom::Value field, dom::Value options);

/** Concatenate strings/arrays. */
dom::Expected<dom::Value>
concat_fn(dom::Array const& arguments);

/** Count occurrences / size, depending on argument kinds. */
std::int64_t
count_fn(dom::Array const& arguments);

/** Replace occurrences within a string or array. */
dom::Value
replace_fn(dom::Array const& arguments);

/** Find the index of a value within a range. */
dom::Value
find_index_fn(dom::Array const& arguments);

} // namespace detail
} // namespace handlebars
} // namespace mrdocs

#endif
