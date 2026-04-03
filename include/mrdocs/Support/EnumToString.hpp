//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SUPPORT_ENUMTOSTRING_HPP
#define MRDOCS_API_SUPPORT_ENUMTOSTRING_HPP

#include <mrdocs/Support/Assert.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <string>

namespace mrdocs {

/** Convert a described enumerator to string form.

    @param e The enumerator to convert.
    @return The string form of the enumerator.
*/
template <typename Enum>
    requires describe::has_describe_enumerators<Enum>::value
std::string
toString(Enum e)
{
    std::string result;
    describe::for_each(
        describe::describe_enumerators<Enum>{},
        [&](auto const& D)
        {
            if (D.value == e)
            {
                result = toKebabCase(D.name);
            }
        });

    if (!result.empty())
    {
        return result;
    }

    MRDOCS_UNREACHABLE();
}

} // namespace mrdocs

#endif // MRDOCS_API_SUPPORT_ENUMTOSTRING_HPP
