//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_TEMPLATE_HPP
#define MRDOCS_API_METADATA_TEMPLATE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Metadata/TArg.hpp>
#include <mrdocs/Metadata/TParam.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <vector>

namespace mrdocs {

/** The kind of template or specialization.
*/
enum class TemplateSpecKind
{
    /// Primary template
    Primary,
    /// Full template specialization
    Explicit,
    /// Partial template specialization
    Partial
};

/** Convert the specialization kind to a readable string.
    @return String view naming the specialization category.
*/
MRDOCS_DECL
std::string_view
toString(TemplateSpecKind kind);


/** Information about templates and specializations thereof.
*/
struct TemplateInfo final
{
    /** Template parameter list.
    */
    std::vector<Polymorphic<TParam>> Params;

    /** Bound arguments for specializations.
    */
    std::vector<Polymorphic<TArg>> Args;

    /** The requires-clause for the template parameter list, if any.
    */
    ExprInfo Requires;

    /** Primary template ID for partial and explicit specializations.
    */
    SymbolID Primary = SymbolID::invalid;

    // KRYSTIAN NOTE: using the presence of args/params
    // to determine the specialization kind *should* work.
    // emphasis on should.
    TemplateSpecKind

    /** Deduce which specialization category this info represents.
    */
    specializationKind() const noexcept
    {
        if (Params.empty())
        {
            return TemplateSpecKind::Explicit;
        }
        if (Args.empty())
        {
            return TemplateSpecKind::Primary;
        }
        return TemplateSpecKind::Partial;
    }

    /** Compare templates by parameters, arguments, and primary.
    */
    std::strong_ordering
    operator<=>(TemplateInfo const& other) const;
};

MRDOCS_DESCRIBE_STRUCT(TemplateInfo, (), (Params, Args, Requires, Primary))

/** Map a TemplateInfo to a dom::Object with computed specialization kind.
    @param io The IO object to map into.
    @param I The TemplateInfo to map.
    @param domCorpus The DomCorpus context.
*/
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    TemplateInfo const& I,
    DomCorpus const* domCorpus)
{
    io.defer("kind", [&] {
        return std::string(toString(I.specializationKind()));
    });
    if (I.Primary != SymbolID::invalid)
    {
        io.map("primary", I.Primary);
    }
    io.map("params", dom::LazyArray(I.Params, domCorpus));
    io.map("args", dom::LazyArray(I.Args, domCorpus));
    io.map("requires", dom::stringOrNull(I.Requires.Written));
}

/** Merge partial template info, filling missing pieces.
*/
MRDOCS_DECL
void
merge(TemplateInfo& I, TemplateInfo&& Other);

/** Compare optional template infos, treating disengaged as ordered before engaged.
*/
inline
auto
operator<=>(Optional<TemplateInfo> const& lhs, Optional<TemplateInfo> const& rhs)
{
    if (!lhs)
    {
        if (!rhs)
        {
            return std::strong_ordering::equal;
        }
        return std::strong_ordering::less;
    }
    if (!rhs)
    {
        return std::strong_ordering::greater;
    }
    return *lhs <=> *rhs;
}

/** Equality helper for optional template info.
*/
inline
bool
operator==(Optional<TemplateInfo> const& lhs, Optional<TemplateInfo> const& rhs)
{
    return lhs <=> rhs == std::strong_ordering::equal;
}

/** Serialize template info into a DOM value.
*/
MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TemplateInfo const& I,
    DomCorpus const* domCorpus);

/** Serialize an optional template info into a DOM value.
*/
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Optional<TemplateInfo> const& I,
    DomCorpus const* domCorpus)
{
    if (!I)
    {
        v = nullptr;
        return;
    }
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}


} // mrdocs

#endif
