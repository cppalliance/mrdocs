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

#ifndef MRDOCS_API_METADATA_TPARAM_NONTYPETPARAM_HPP
#define MRDOCS_API_METADATA_TPARAM_NONTYPETPARAM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/TParam/TParamBase.hpp>
#include <mrdocs/Metadata/Type/TypeBase.hpp>

namespace clang::mrdocs {

struct NonTypeTParam final
    : TParamCommonBase<TParamKind::NonType>
{
    /** Type of the non-type template parameter */
    Optional<Polymorphic<TypeInfo>> Type = std::nullopt;

    std::strong_ordering operator<=>(NonTypeTParam const&) const;
};

} // clang::mrdocs

#endif
