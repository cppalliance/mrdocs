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
    std::unique_ptr<NameInfo> Prefix;

    constexpr bool isIdentifier()     const noexcept { return Kind == NameKind::Identifier; }
    constexpr bool isSpecialization() const noexcept { return Kind == NameKind::Specialization; }

    constexpr
    NameInfo() noexcept
        : NameInfo(NameKind::Identifier) {}

    explicit
    constexpr
    NameInfo(NameKind kind) noexcept
        : Kind(kind) {}

    virtual ~NameInfo() = default;
};

/** Represents a (possibly qualified) symbol name with template arguments.
*/
struct SpecializationNameInfo
    : NameInfo
{
    /** The template arguments.
    */
    std::vector<std::unique_ptr<TArg>> TemplateArgs;

    constexpr
    SpecializationNameInfo() noexcept
        : NameInfo(NameKind::Specialization)
    {}
};

template<
    class NameInfoTy,
    class Fn,
    class... Args>
    requires std::derived_from<NameInfoTy, NameInfo>
decltype(auto)
visit(
    NameInfoTy& info,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<NameInfo>(
        info, std::forward<Fn>(fn),
        std::forward<Args>(args)...);
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

MRDOCS_DECL
std::string
toString(const NameInfo& N);

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
    std::unique_ptr<NameInfo> const& I,
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
