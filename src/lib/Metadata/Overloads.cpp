//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata/Function.hpp>
#include <mrdocs/Metadata/Namespace.hpp>
#include <mrdocs/Metadata/Overloads.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <lib/Dom/LazyArray.hpp>
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/StringRef.h>
#include "lib/Support/Radix.hpp"

namespace clang::mrdocs {

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    OverloadSet const& overloads,
    DomCorpus const* domCorpus)
{
    /* Unfortunately, this can't use LazyObject like all other
     * Corpus types because the overload sets are not directly
     * available from the corpus.
     * The `overloads` value is a temporary reference created
     * by the `Info` tag_invoke.
     */
    dom::Object res;
    res.set("id", fmt::format("{}-{}", toBase16(overloads.Parent), overloads.Name));
    res.set("kind", "overloads");
    res.set("name", overloads.Name);
    res.set("members", dom::LazyArray(overloads.Members, domCorpus));

    // Copy other redundant fields from the first member
    if (overloads.Members.size() > 0)
    {
        dom::Value const member = domCorpus->get(overloads.Members[0]);
        if (member.isObject())
        {
            for (std::string_view const key:
                 { "parent",
                   "parents",
                   "access",
                   "extraction",
                   "isRegular",
                   "isSeeBelow",
                   "isImplementationDefined",
                   "isDependency" })
            {
                res.set(key, member.get(key));
            }
        }
    }

    // Copy relevant values from the first member with documentation
    // that contains it.
    for (std::string_view const key: {"doc", "loc", "dcl"})
    {
        for (std::size_t i = 0; i < overloads.Members.size(); ++i)
        {
            dom::Value member = domCorpus->get(overloads.Members[i]);
            if (member.isObject() && member.getObject().exists(key))
            {
                res.set(key, member.get(key));
                break;
            }
        }
    }
    v = res;
}

} // clang::mrdocs
