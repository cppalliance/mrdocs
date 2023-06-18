//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "-adoc/DocVisitor.hpp"
#include <mrdox/Dom/DomJavadoc.hpp>

namespace clang {
namespace mrdox {

DomJavadoc::
DomJavadoc(
    Javadoc const& jd,
    Corpus const& corpus) noexcept
    : jd_(&jd)
    , corpus_(corpus)
{
}

dom::Value
DomJavadoc::
get(std::string_view key) const
{
    if(key == "brief")
    {
        std::string s;
        adoc::DocVisitor visitor(s);
        s.clear();
        if(auto brief = jd_->getBrief())
            visitor(*brief);
        return dom::nonEmptyString(s);
    }
    if(key == "description")
    {
        if(jd_->getBlocks().empty())
            return nullptr;
        std::string s;
        adoc::DocVisitor visitor(s);
        visitor(jd_->getBlocks());
        return dom::nonEmptyString(s);
    }
    return nullptr;
}

auto
DomJavadoc::
props() const ->
    std::vector<std::string_view>
{
    return {
        "brief",
        "description"
    };
}

} // mrdox
} // clang
