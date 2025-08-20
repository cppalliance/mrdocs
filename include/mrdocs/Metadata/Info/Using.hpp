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

    For instance, the following code:

    @code
    using A::f; // where f is a function in namespace A
    @endcode

    would be represented by a `UsingInfo` object.

    Using-declarations can be used to introduce namespace
    members into other namespaces and block scopes, or to
    introduce base class members into derived class definitions,
    or to introduce enumerators into namespaces, block,
    and class scopes.

 */
struct UsingInfo final
    : InfoCommonBase<InfoKind::Using>
{
    /** The using declaration.
    */
    UsingClass Class = UsingClass::Normal;

    /** The symbol being introduced.

        This is the symbol that is being "used" or
        introduced into the current scope.

        Note that this can be a qualified name, such as
        `A::f` in the example above.
     */
    Polymorphic<NameInfo> IntroducedName;

    /** The shadow declarations.

        A using declaration can refer to and introduce
        multiple symbols into the current context.

        These multiple symbols are considered a special
        case of declarations: "shadow declarations".

        This typically happens when there are
        conflicting symbol names in the scope
        being introduced, such as:

        * Overloaded functions: the base namespace contains
          overloaded functions.
        * Name conflicts: the base scope contains a function
          and a type.
        * Enums: a using enum declaration can refer to
          multiple enumerators.

       Also note that more shadow declarations can be
       introduced later in the same scope, after
       the using declaration.

       The shadow declarations here are only those
       that are shadowed at the point where the
       using declaration is located.
    */
    std::vector<SymbolID> ShadowDeclarations;

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
    io.map("shadows", dom::LazyArray(I.ShadowDeclarations, domCorpus));
    io.map("qualifier", I.IntroducedName);
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
