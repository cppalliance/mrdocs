//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_TYPEDEF_HPP
#define MRDOCS_API_METADATA_SYMBOL_TYPEDEF_HPP

#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace mrdocs {

// Info for typedef and using statements.
struct TypedefSymbol final
    : SymbolCommonBase<SymbolKind::Typedef>
{
    Polymorphic<struct Type> Type = Polymorphic<struct Type>(NamedType{});

    /** Indicates if this is a new C++ "using"-style typedef

        @code
        using MyVector = std::vector<int>
        @endcode

        False means it's a C-style typedef:

        @code
        typedef std::vector<int> MyVector;
        @endcode
      */
    bool IsUsing = false;

    Optional<TemplateInfo> Template;

    //--------------------------------------------

    explicit TypedefSymbol(SymbolID ID) noexcept
        : SymbolCommonBase(ID)
    {
    }

    std::strong_ordering
    operator<=>(TypedefSymbol const& other) const;

};

MRDOCS_DECL
void
merge(TypedefSymbol& I, TypedefSymbol&& Other);

/** Map a TypedefSymbol to a dom::Object.

    @param t The tag type.
    @param io The IO object to use for mapping.
    @param I The TypedefSymbol to map.
    @param domCorpus The DomCorpus used to create
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    TypedefSymbol const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, I.asInfo(), domCorpus);
    io.map("type", I.Type);
    io.map("template", I.Template);
    io.map("isUsing", I.IsUsing);
}

/** Map the TypedefSymbol to a @ref dom::Value object.

    @param v The output parameter to receive the dom::Value.
    @param I The TypedefSymbol to convert.
    @param domCorpus The DomCorpus used to resolve references.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TypedefSymbol const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_TYPEDEF_HPP
