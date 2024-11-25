//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Scope.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Dom.hpp>

namespace clang {
namespace mrdocs {

dom::Array
generateScopeOverloadsArray(
    ScopeInfo const& I,
    DomCorpus const& domCorpus)
{
    /* Unfortunately, this information is not readily
       available in the Corpus, so we can't have lazy
       references to these members like we do for
       other Info types.
     */
    dom::Array res;
    domCorpus.getCorpus().traverseOverloads(I, [&](const auto& C)
    {
        using BareT = std::remove_cvref_t<decltype(C)>;
        static_assert(std::is_base_of_v<Info, BareT> || std::is_same_v<OverloadSet, BareT>);
        if constexpr(std::is_base_of_v<Info, BareT>)
        {
            res.push_back(domCorpus.get(C.id));
        }
        else /* if constexpr(std::is_same_v<OverloadSet, BareT>) */
        {
            res.push_back(domCorpus.construct(C));
        }
    });
    return res;
}


} // mrdocs
} // clang
