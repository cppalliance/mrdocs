//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_FORWARD_HPP
#define MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_FORWARD_HPP

#include <mrdocs/Dom/Array.hpp>
#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>
#include <mrdocs/Support/String/String.hpp>
#include <mrdocs/Support/TypeTraits/TypeTraits.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Only <mrdocs/Support/DescribedToDom.hpp> is meant to be included
// directly. This header and the other DescribedToDom/ headers are its
// pieces, included by it in an order their mutual references require, and
// including one on its own may not compile. It holds the forward
// declarations that break the cycle between the proxies and
// `describedToDom`: each proxy converts its members with
// `describedToDom`, and `describedToDom` in turn builds those
// proxies, so all three are declared here before any is defined. The full
// documentation lives with the definitions in DescribedToDom.hpp.
namespace mrdocs {

template <class T> class DescribedObjectProxy;

template <class Vec>
requires mrdocs::specialization_of<std::remove_const_t<Vec>, std::vector>
class DescribedArrayProxy;

template <class T>
dom::Value
describedToDom(T& value);

} // mrdocs

#endif // MRDOCS_LIB_SUPPORT_DESCRIBEDTODOM_FORWARD_HPP
