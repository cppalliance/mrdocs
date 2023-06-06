//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_SPECIALIZATION_HPP
#define MRDOX_API_METADATA_SPECIALIZATION_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <mrdox/Metadata/Info.hpp>
#include <mrdox/Metadata/Symbols.hpp>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

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
};

/** Specialization info for members of implicit instantiations
*/
struct SpecializationInfo
    : Info
{
    /** The template arguments the parent template is specialized for */
    std::vector<TArg> Args;

    /** ID of the template to which the arguments pertain */
    SymbolID Primary = SymbolID::zero;

    /** The specialized members.

        A specialized member `C` may itself be a `SpecializationInfo`
        if any of its members `M` are explicitly specialized for an implicit
        instantiation of `C`.
    */
    std::vector<SpecializedMember> Members;

    static constexpr InfoKind kind_id = InfoKind::Specialization;

    SpecializationInfo(
        SymbolID ID = SymbolID::zero)
        : Info(InfoKind::Specialization, ID)
    {
    }
};

} // mrdox
} // clang

#endif
