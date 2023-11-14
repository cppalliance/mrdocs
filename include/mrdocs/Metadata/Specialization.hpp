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
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Scope.hpp>
#include <utility>
#include <vector>

namespace clang {
namespace mrdocs {

/** Specialization info for members of implicit instantiations
*/
struct SpecializationInfo
    : IsInfo<InfoKind::Specialization>
    , ScopeInfo
{
    /** The template arguments the parent template is specialized for */
    std::vector<std::unique_ptr<TArg>> Args;

    /** ID of the template to which the arguments pertain */
    SymbolID Primary = SymbolID::invalid;

    explicit SpecializationInfo(SymbolID ID) noexcept
        : IsInfo(ID)
    {
    }
};

} // mrdocs
} // clang

#endif
