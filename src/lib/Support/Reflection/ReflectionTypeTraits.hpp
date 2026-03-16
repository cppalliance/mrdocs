//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_REFLECTION_REFLECTIONTYPETRAITS_HPP
#define MRDOCS_LIB_SUPPORT_REFLECTION_REFLECTIONTYPETRAITS_HPP

#include <mrdocs/ADT/Optional.hpp>
#include <type_traits>
#include <vector>

namespace mrdocs::detail {

template <typename T>
struct is_optional : std::false_type {};

template <typename T>
struct is_optional<Optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

template <typename T>
struct is_vector : std::false_type {};

template <typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};

template <typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

}

#endif
