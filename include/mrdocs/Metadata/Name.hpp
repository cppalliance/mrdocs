//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
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

namespace clang {
namespace mrdocs {

enum class NameKind
{
    Identifier = 1, // for bitstream
    Specialization
};

MRDOCS_DECL
dom::String
toString(NameKind kind) noexcept;

/** Represents a (possibly qualified) symbol name.
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
    */
    std::unique_ptr<NameInfo> Prefix;

    constexpr bool isIdentifier()     const noexcept { return Kind == NameKind::Identifier; }
    constexpr bool isSpecialization() const noexcept { return Kind == NameKind::Specialization; }

    constexpr
    NameInfo() noexcept
        : NameInfo(NameKind::Identifier)
    {
    }

    constexpr
    NameInfo(NameKind kind) noexcept
        : Kind(kind)
    {
    }

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
    {
    }
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

} // mrdocs
} // clang

#endif
