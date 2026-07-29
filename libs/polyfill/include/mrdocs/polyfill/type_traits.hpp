//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_POLYFILL_TYPE_TRAITS_HPP
#define MRDOCS_POLYFILL_TYPE_TRAITS_HPP

// Self-contained (std-only) type traits the polyfills need. // mrdocs::polyfill::detail, so the polyfills depend on nothing but the standard
// library. These mirror the C++23 reference-from-temporary traits and become
// aliases to the std versions when available.

#include <type_traits>
#include <version>

namespace mrdocs::polyfill {

#ifdef __cpp_lib_reference_from_temporary
using std::reference_constructs_from_temporary_v;
using std::reference_converts_from_temporary_v;
#else
template <class To, class From>
concept reference_converts_from_temporary_v
    = std::is_reference_v<To>
      && ((!std::is_reference_v<From>
           && std::is_convertible_v<
               std::remove_cvref_t<From>*,
               std::remove_cvref_t<To>*>)
          || (std::is_lvalue_reference_v<To>
              && std::is_const_v<std::remove_reference_t<To>>
              && std::is_convertible_v<From, const std::remove_cvref_t<To>&&>
              && !std::is_convertible_v<From, std::remove_cvref_t<To>&>) );

template <class To, class From>
concept reference_constructs_from_temporary_v
    = reference_converts_from_temporary_v<To, From>;
#endif

} // namespace mrdocs::polyfill

#endif
