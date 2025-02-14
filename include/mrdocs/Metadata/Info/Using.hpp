//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2024 Fernando Pelliccioni (fpelliccioni@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_USING_HPP
#define MRDOCS_API_METADATA_USING_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <vector>

namespace clang::mrdocs {

enum class UsingClass
{
    Normal = 0,     // using
    Typename,       // using typename
    Enum            // using enum
};

static constexpr
std::string_view
toString(UsingClass const& value)
{
    switch (value)
    {
    case UsingClass::Normal:    return "normal";
    case UsingClass::Typename:  return "typename";
    case UsingClass::Enum:      return "enum";
    }
    return "unknown";
}

/** Return the UsingClass as a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    UsingClass kind)
{
    v = toString(kind);
}


/** Info for using declarations.
 */
struct UsingInfo final
    : InfoCommonBase<InfoKind::Using>
{
    /** The kind of using declaration.
    */
    UsingClass Class = UsingClass::Normal;

    /** The symbols being "used".
    */
    std::vector<SymbolID> UsingSymbols;

    /** The qualifier for a using declaration.
    */
    Polymorphic<NameInfo> Qualifier;

    //--------------------------------------------

    explicit UsingInfo(SymbolID ID) noexcept
        : InfoCommonBase(ID)
    {
    }
};

MRDOCS_DECL
void merge(UsingInfo& I, UsingInfo&& Other);

/** Map a UsingInfo to a dom::Object.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    UsingInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    io.map("usingClass", I.Class);
    io.map("shadows", dom::LazyArray(I.UsingSymbols, domCorpus));
    io.map("qualifier", I.Qualifier);
}

/** Map the UsingInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    UsingInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif
