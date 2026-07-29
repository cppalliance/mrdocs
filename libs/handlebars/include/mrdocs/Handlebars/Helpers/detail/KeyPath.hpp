//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_HELPERS_DETAIL_KEYPATH_HPP
#define MRDOCS_API_HANDLEBARS_HELPERS_DETAIL_KEYPATH_HPP

// Internal object key-path utilities, shared by the constructor and container
// helper categories. Not part of the public API.

#include <mrdocs/Dom.hpp>
#include <string>
#include <vector>

namespace mrdocs {
namespace handlebars {
namespace detail {

/** Look up a nested value in an object by a sequence of keys. */
dom::Value
getNestedValue(dom::Value const& obj, std::vector<std::string> const& keys);

/** Split a dotted key path into its components. */
std::vector<std::string>
parseKeyPath(std::string const& keyPath);

} // namespace detail
} // namespace handlebars
} // namespace mrdocs

#endif
