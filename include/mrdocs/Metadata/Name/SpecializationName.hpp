//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_NAME_SPECIALIZATIONNAME_HPP
#define MRDOCS_API_METADATA_NAME_SPECIALIZATIONNAME_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/TArg.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

/** Represents a (possibly qualified) symbol name with template arguments.
*/
struct SpecializationName final
    : Name
{
    /** The variant discriminator constant of this concrete name kind. */
    static constexpr NameKind kind_id = NameKind::Specialization;

    /** The template arguments.
    */
    std::vector<Polymorphic<TArg>> TemplateArgs;

    /** The SymbolID of the named symbol, if it exists.
    */
    SymbolID specializationID = SymbolID::invalid;

    /** Construct an empty specialization name.
    */
    constexpr
    SpecializationName() noexcept
        : Name(NameKind::Specialization)
    {}

    /** Compare specialization names by base name and template arguments.
    */
    auto
    operator<=>(SpecializationName const& other) const
    {
        if (auto const r = asName() <=> other.asName();
            !std::is_eq(r))
        {
            return r;
        }
        return TemplateArgs <=> other.TemplateArgs;
    }
};

MRDOCS_DESCRIBE_STRUCT(SpecializationName, (Name), (TemplateArgs))

} // mrdocs

#endif // MRDOCS_API_METADATA_NAME_SPECIALIZATIONNAME_HPP
