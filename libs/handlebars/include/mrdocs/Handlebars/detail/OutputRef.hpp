//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_HANDLEBARS_DETAIL_OUTPUTREF_HPP
#define MRDOCS_API_HANDLEBARS_DETAIL_OUTPUTREF_HPP

// Concepts used by OutputRef to accept the various stream-like sinks it wraps.
// Internal to the handlebars library.

#include <concepts>
#include <ostream>
#include <string_view>
#include <type_traits>

namespace mrdocs {
namespace handlebars {
namespace detail {

// Objects such as llvm::raw_string_ostream
template <typename Os>
concept LHROStreamable =
    requires(Os &os, std::string_view sv)
{
    { os << sv } -> std::convertible_to<Os &>;
};

// Objects such as std::ofstream
template <typename Os>
concept StdLHROStreamable = LHROStreamable<Os> && std::convertible_to<Os*, std::ostream*>;

// Objects such as std::string
template <typename St>
concept SVAppendable =
    requires(St &st, std::string_view sv)
{
    st.append( sv.data(), sv.data() + sv.size() );
};

} // namespace detail
} // namespace handlebars
} // namespace mrdocs

#endif
