//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SPECIALIZATION_HPP
#define MRDOCS_API_METADATA_SPECIALIZATION_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/MetadataFwd.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <utility>
#include <vector>

namespace clang {
namespace mrdocs {

/** Primary and specialized IDs of specialized members
*/
struct SpecializedMember
{
    /** ID of the member in the primary template */
    SymbolID Primary;

    /** ID of the member specialization */
    SymbolID Specialized;

    SpecializedMember() = default;

    SpecializedMember(
        SymbolID primary,
        SymbolID specialized)
        : Primary(primary)
        , Specialized(specialized)
    {
    }

    bool operator==(const SpecializedMember&) const = default;
};

/** Specialization info for members of implicit instantiations
*/
struct SpecializationInfo
    : IsInfo<InfoKind::Specialization>
{
    /** The template arguments the parent template is specialized for */
    std::vector<std::unique_ptr<TArg>> Args;

    /** ID of the template to which the arguments pertain */
    SymbolID Primary = SymbolID::invalid;

    /** The specialized members.

        A specialized member `C` may itself be a `SpecializationInfo`
        if any of its members `M` are explicitly specialized for an implicit
        instantiation of `C`.
    */
    std::vector<SpecializedMember> Members;

    explicit
    SpecializationInfo(
        SymbolID ID = SymbolID::invalid)
        : IsInfo(ID)
    {
    }
};

} // mrdocs
} // clang

#endif
