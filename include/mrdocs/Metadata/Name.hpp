//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_NAME_HPP
#define MRDOCS_API_METADATA_NAME_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata/Name/IdentifierName.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/Name/SpecializationName.hpp>
#include <mrdocs/Support/Reflection/MapReflectedType.hpp>
#include <mrdocs/Support/TypeTraits/Visitor.hpp>

namespace mrdocs {

template<
    std::derived_from<Name> NameTy,
    class Fn,
    class... Args>
decltype(auto)
visit(
    NameTy& info,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<Name>(
        info, std::forward<Fn>(fn), std::forward<Args>(args)...);
    switch(info.Kind)
    {
    #define INFO(Type) case NameKind::Type: \
        return visitor.template visit<Type##Name>();
#include <mrdocs/Metadata/Name/NameNodes.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Map a Name to a dom::Object via visit-based polymorphic dispatch.
    @param io The IO object to map into.
    @param I The Name to map.
    @param domCorpus The DomCorpus context.
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    Name const& I,
    DomCorpus const* domCorpus)
{
    addMetaObject<Name>(io);
    io.map("kind", I.Kind);
    visit(I, [domCorpus, &io]<typename T>(T const& t)
    {
        io.map("name", t.Identifier);
        io.map("id", t.id);
        if constexpr(requires { t.TemplateArgs; })
        {
            io.map("args", dom::LazyArray(t.TemplateArgs, domCorpus));
        }
        io.map("prefix", t.Prefix);
    });
}


/** Serialize a polymorphic name into a DOM value.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<Name> const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(!I.valueless_after_move());
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

/** Serialize an optional polymorphic name into a DOM value.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Optional<Polymorphic<Name>> const& I,
    DomCorpus const* domCorpus)
{
    if (!I)
    {
        v = nullptr;
        return;
    }
    MRDOCS_ASSERT(!I->valueless_after_move());
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}

} // mrdocs

#endif
