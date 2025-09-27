//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_NAME_SPECIALIZATIONNAMEINFO_HPP
#define MRDOCS_API_METADATA_NAME_SPECIALIZATIONNAMEINFO_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Name/NameBase.hpp>
#include <mrdocs/Metadata/TArg.hpp>

namespace clang::mrdocs {

/** Represents a (possibly qualified) symbol name with template arguments.
*/
struct SpecializationNameInfo final
    : NameInfo
{
    /** The template arguments.
    */
    std::vector<Polymorphic<TArg>> TemplateArgs;

    /** The SymbolID of the named symbol, if it exists.
    */
    SymbolID specializationID = SymbolID::invalid;

    constexpr
    SpecializationNameInfo() noexcept
        : NameInfo(NameKind::Specialization)
    {}

    auto
    operator<=>(SpecializationNameInfo const& other) const
    {
        if (auto const r = asName() <=> other.asName();
            !std::is_eq(r))
        {
            return r;
        }
        return TemplateArgs <=> other.TemplateArgs;
    }
};

} // clang::mrdocs

#endif
