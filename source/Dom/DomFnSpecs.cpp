//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/DomFnSpecs.hpp>

namespace clang {
namespace mrdox {

DomFnSpecs::
DomFnSpecs(
    FunctionInfo const& I,
    Corpus const& corpus) noexcept
    : specs0_(I.specs0)
    , specs1_(I.specs1)
    , corpus_(corpus)
{
}

dom::Value
DomFnSpecs::
get(std::string_view key) const
{
    if(key == "isVariadic")
        return specs0_.isVariadic.get();
    if(key == "isVirtual")
        return specs0_.isVirtual.get();
    if(key == "isVirtualAsWritten")
        return specs0_.isVirtualAsWritten.get();
    if(key == "isPure")
        return specs0_.isPure.get();
    if(key == "isDefaulted")
        return specs0_.isDefaulted.get();
    if(key == "isExplicitlyDefaulted")
        return specs0_.isExplicitlyDefaulted.get();
    if(key == "isDeleted")
        return specs0_.isDeleted.get();
    if(key == "isDeletedAsWritten")
        return specs0_.isDeletedAsWritten.get();
    if(key == "isNoReturn")
        return specs0_.isNoReturn.get();
    if(key == "hasOverrideAttr")
        return specs0_.hasOverrideAttr.get();
    if(key == "hasTrailingReturn")
        return specs0_.hasTrailingReturn.get();
    if(key == "isConst")
        return specs0_.isConst.get();
    if(key == "isVolatile")
        return specs0_.isVolatile.get();
    if(key == "isFinal")
        return specs0_.isFinal.get();
    if(key == "isNodiscard")
        return specs1_.isNodiscard.get();

    if(key == "constexprKind")
        return toString(specs0_.constexprKind.get());
    if(key == "exceptionSpec")
        return toString(specs0_.exceptionSpec.get());
    if(key == "overloadedOperator")
        return specs0_.overloadedOperator.get();
    if(key == "storageClass")
        return toString(specs0_.storageClass.get());
    if(key == "refQualifier")
        return toString(specs0_.refQualifier.get());
    if(key == "explicitSpec")
        return toString(specs1_.explicitSpec.get());

    return nullptr;
}

auto
DomFnSpecs::
props() const ->
    std::vector<std::string_view>
{
    return {
        "isVariadic",
        "isVirtual",
        "isVirtualAsWritten",
        "isPure",
        "isDefaulted",
        "isExplicitlyDefaulted",
        "isDeleted",
        "isDeletedAsWritten",
        "isNoReturn",
        "hasOverrideAttr",
        "hasTrailingReturn",
        "isConst",
        "isVolatile",
        "isFinal",
        "isNodiscard",

        "constexprKind",
        "exceptionSpec",
        "overloadedOperator",
        "storageClass",
        "refQualifier",
        "explicitSpec"
    };
}

} // mrdox
} // clang
