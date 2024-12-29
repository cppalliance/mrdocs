//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_CONCEPTS_HPP
#define MRDOCS_API_SUPPORT_CONCEPTS_HPP

#include <ranges>

namespace clang::mrdocs {

template <typename Range, typename T>
concept range_of = std::ranges::range<Range> && std::same_as<std::ranges::range_value_t<Range>, T>;

} // namespace clang::mrdocs


#endif // MRDOCS_API_SUPPORT_CONCEPTS_HPP
