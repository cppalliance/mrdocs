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

namespace clang {
namespace mrdocs {

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
    res.set("namespace", dom::LazyArray(overloads.Namespace, domCorpus));
    res.set("parent", domCorpus->get(overloads.Parent));
    dom::Value firstM = domCorpus->get(overloads.Members.front());
    if (firstM.isObject() && firstM.get("doc").isObject())
    {
        res.set("doc", firstM.get("doc"));
    }
    v = res;
}

} // mrdocs
} // clang
