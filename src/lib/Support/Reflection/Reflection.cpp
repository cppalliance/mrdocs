//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "MergeReflectedType.hpp"
#include "Reflection.hpp"
#include "MapReflectedType.hpp"
#include <mrdocs/Metadata/Type/NamedType.hpp>

namespace mrdocs {

namespace detail {

bool
isPlaceholderType(Polymorphic<Type> const& t)
{
    return t->isAuto() ||
        (t->isNamed() &&
         t->asNamed().Name->Identifier.empty());
}

} // namespace detail

//------------------------------------------------------
// Symbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    Symbol const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(domCorpus);
    // Note: Symbol is always a base class, never most-derived,
    // so we don't add $meta here. The derived type adds it.
    mapReflectedType<false>(io, I, domCorpus);
    io.map("class", std::string("symbol"));
    io.map("isRegular", I.Extraction == ExtractionMode::Regular);
    io.map("isSeeBelow", I.Extraction == ExtractionMode::SeeBelow);
    io.map("isImplementationDefined", I.Extraction == ExtractionMode::ImplementationDefined);
    io.map("isDependency", I.Extraction == ExtractionMode::Dependency);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    Symbol const&,
    DomCorpus const*);

//------------------------------------------------------
// ImageInline.
//------------------------------------------------------

template <typename IO>
void
doc::tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    doc::ImageInline const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
doc::tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    doc::ImageInline const&,
    DomCorpus const*);

//------------------------------------------------------
// ConceptSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    ConceptSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    ConceptSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// DocComment.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    DocComment const& I,
    DomCorpus const* domCorpus)
{
    addMetaObject<DocComment>(io);

    boost::mp11::mp_for_each<boost::describe::describe_members<
        DocComment, boost::describe::mod_public>>([&](auto D)
        {
            constexpr std::string_view name = D.name;

            if constexpr (name == "Document")
            {
                io.defer("description", [&I, domCorpus]
                {
                    return dom::LazyArray(I.Document, domCorpus);
                });
            }
            else if constexpr (name == "brief")
            {
                if (I.brief && !I.brief->children.empty())
                {
                    io.map("brief", I.brief);
                }
            }
            else
            {
                io.defer(D.name, [&I, domCorpus, ptr = D.pointer]
                {
                    return dom::LazyArray(I.*ptr, domCorpus);
                });
            }
        });
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    DocComment const&,
    DomCorpus const*
);

//------------------------------------------------------
// EnumConstantSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    EnumConstantSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    EnumConstantSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// EnumSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    EnumSymbol const& I,
    DomCorpus const* domCorpus)
{
    addMetaObject<EnumSymbol>(io);

    tag_invoke(dom::LazyObjectMapTag{}, io, I.asInfo(), domCorpus);
    io.map("type", I.UnderlyingType);
    io.map("isScoped", I.Scoped);
    io.map("constants", dom::LazyArray(I.Constants, domCorpus));
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    EnumSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// FunctionSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    FunctionSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    FunctionSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// GuideSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    GuideSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    GuideSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// NamespaceTranche.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    NamespaceTranche const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    NamespaceTranche const&,
    DomCorpus const*);

//------------------------------------------------------
// NamespaceSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    NamespaceSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    NamespaceSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// NamespaceAliasSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    NamespaceAliasSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    NamespaceAliasSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// OverloadsSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    OverloadsSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    OverloadsSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// RecordTranche.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    RecordTranche const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    RecordTranche const&,
    DomCorpus const*);

//------------------------------------------------------
// RecordInterface.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    RecordInterface const& I,
    DomCorpus const*)
{
    mapReflectedType<true>(io, I);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    RecordInterface const&,
    DomCorpus const*);

//------------------------------------------------------
// RecordSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    RecordSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
    io.map("defaultAccess", getDefaultAccessString(I.KeyKind));
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    RecordSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// TypedefSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    TypedefSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    TypedefSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// UsingSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    UsingSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    UsingSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// VariableSymbol.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    VariableSymbol const& I,
    DomCorpus const* domCorpus)
{
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    VariableSymbol const&,
    DomCorpus const*);

//------------------------------------------------------
// FriendInfo.
//------------------------------------------------------

template <typename IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    FriendInfo const& I,
    DomCorpus const* domCorpus)
{
    if (I.id)
    {
        io.defer("name", [&I, domCorpus]{
            return dom::ValueFrom(I.id, domCorpus).get("name");
        });
        io.map("symbol", I.id);
    }
    else if (I.Type)
    {
        io.defer("name", [&]{
            return dom::ValueFrom(I.Type, domCorpus).get("name");
        });
    }
    mapReflectedType<true>(io, I, domCorpus);
}

template
void
tag_invoke<LazyObjectIOType>(
    dom::LazyObjectMapTag,
    LazyObjectIOType&,
    FriendInfo const&,
    DomCorpus const*);

} // mrdocs
