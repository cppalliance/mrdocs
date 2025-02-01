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
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <memory>

namespace clang::mrdocs {

enum class NameKind
{
    Identifier = 1, // for bitstream
    Specialization
};

MRDOCS_DECL
dom::String
toString(NameKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    NameKind const kind)
{
    v = toString(kind);
}

/** Represents a name for a named `TypeInfo`

    When the `TypeInfo` is a named type, this class
    represents the name of the type.

    It also includes the symbol ID of the named type,
    so that it can be referenced in the documentation.

    This allows the `TypeInfo` to store either a
    `NameInfo` or a `SpecializationNameInfo`,
    which contains the arguments for a template specialization
    without requiring the application to extract an
    unnecessary symbol.
 */
struct NameInfo
{
    /** The kind of name this is.
    */
    NameKind Kind;

    /** The SymbolID of the named symbol, if it exists.
    */
    SymbolID id = SymbolID::invalid;

    /** The unqualified name.
    */
    std::string Name;

    /** The parent name info, if any.

        This recursively includes information about
        the parent, such as the symbol ID and
        potentially template arguments, when
        the parent is a SpecializationNameInfo.

        This is particularly useful because the
        parent of `id` could be a primary template.
        In this case, the Prefix will contain
        this primary template information
        and the template arguments.

     */
    Polymorphic<NameInfo> Prefix;

    constexpr bool isIdentifier()     const noexcept { return Kind == NameKind::Identifier; }
    constexpr bool isSpecialization() const noexcept { return Kind == NameKind::Specialization; }

    constexpr
    NameInfo() noexcept
        : NameInfo(NameKind::Identifier) {}

    explicit
    constexpr
    NameInfo(NameKind const kind) noexcept
        : Kind(kind) {}

    constexpr virtual ~NameInfo() = default;

    std::strong_ordering
    operator<=>(NameInfo const& other) const;
};

/** Represents a (possibly qualified) symbol name with template arguments.
*/
struct SpecializationNameInfo final
    : NameInfo
{
    /** The template arguments.
    */
    std::vector<Polymorphic<TArg>> TemplateArgs;

    constexpr
    SpecializationNameInfo() noexcept
        : NameInfo(NameKind::Specialization)
    {}

    auto
    operator<=>(SpecializationNameInfo const& other) const
    {
        if (auto const r =
            dynamic_cast<NameInfo const&>(*this) <=> dynamic_cast<NameInfo const&>(other);
            !std::is_eq(r))
        {
            return r;
        }
        return TemplateArgs <=> other.TemplateArgs;
    }
};

template<
    std::derived_from<NameInfo> NameInfoTy,
    class Fn,
    class... Args>
decltype(auto)
visit(
    NameInfoTy& info,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<NameInfo>(
        info, std::forward<Fn>(fn), std::forward<Args>(args)...);
    switch(info.Kind)
    {
    case NameKind::Identifier:
        return visitor.template visit<NameInfo>();
    case NameKind::Specialization:
        return visitor.template visit<SpecializationNameInfo>();
    default:
        MRDOCS_UNREACHABLE();
    }
}

inline
std::strong_ordering
operator<=>(Polymorphic<NameInfo> const& lhs, Polymorphic<NameInfo> const& rhs)
{
    return CompareDerived(lhs, rhs);
}

inline
bool
operator==(Polymorphic<NameInfo> const& lhs, Polymorphic<NameInfo> const& rhs) {
    return lhs <=> rhs == std::strong_ordering::equal;
}

MRDOCS_DECL
std::string
toString(NameInfo const& N);

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    NameInfo const& I,
    DomCorpus const* domCorpus);

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Polymorphic<NameInfo> const& I,
    DomCorpus const* domCorpus)
{
    if(! I)
    {
        v = nullptr;
        return;
    }
    tag_invoke(dom::ValueFromTag{}, v, *I, domCorpus);
}


} // clang::mrdocs

#endif
