//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_VARIABLE_HPP
#define MRDOCS_API_METADATA_VARIABLE_HPP

#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>

namespace clang::mrdocs {

/** A variable.

    This includes variables at namespace
    scope, and static variables at class scope.
*/
struct VariableInfo final
    : InfoCommonBase<InfoKind::Variable>
{
    /** The type of the variable */
    Polymorphic<TypeInfo> Type;

    std::optional<TemplateInfo> Template;

    ExprInfo Initializer;

    StorageClassKind StorageClass = StorageClassKind::None;

    bool IsInline = false;

    bool IsConstexpr = false;

    bool IsConstinit = false;

    bool IsThreadLocal = false;

    std::vector<std::string> Attributes;

    //--------------------------------------------

    explicit VariableInfo(SymbolID const &ID) noexcept
        : InfoCommonBase(ID)
    {
    }

    std::strong_ordering
    operator<=>(VariableInfo const& other) const;
};

MRDOCS_DECL
void
merge(VariableInfo& I, VariableInfo&& Other);

/** Map a VariableInfo to a dom::Object.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    VariableInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    io.map("type", I.Type);
    io.map("template", I.Template);
    if (I.StorageClass != StorageClassKind::None)
    {
        io.map("storageClass", I.StorageClass);
    }
    io.map("isInline", I.IsInline);
    io.map("isConstexpr", I.IsConstexpr);
    io.map("isConstinit", I.IsConstinit);
    io.map("isThreadLocal", I.IsThreadLocal);
    if (!I.Initializer.Written.empty())
    {
        io.map("initializer", I.Initializer.Written);
    }
    io.map("attributes", dom::LazyArray(I.Attributes));
}

/** Map the VariableInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    VariableInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif
