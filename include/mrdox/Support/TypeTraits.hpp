//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_SUPPORT_TYPE_TRAITS_HPP
#define MRDOX_TOOL_SUPPORT_TYPE_TRAITS_HPP

#include <type_traits>

namespace clang {
namespace mrdox {

/** Return the value as its underlying type.
*/
template<class Enum>
requires std::is_enum_v<Enum>
constexpr auto
to_underlying(
    Enum value) noexcept ->
    std::underlying_type_t<Enum>
{
    return static_cast<
        std::underlying_type_t<Enum>>(value);
}

template<typename T, typename U>
struct make_dependent
{
    using type = T;
};

template<typename T, typename U>
using make_dependent_t = make_dependent<T, U>::type;

} // mrdox
} // clang

#endif
