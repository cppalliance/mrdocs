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

#ifndef MRDOCS_API_METADATA_TARG_TYPETARG_HPP
#define MRDOCS_API_METADATA_TARG_TYPETARG_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/TArg/TArgBase.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace clang::mrdocs {

struct TypeTArg final
    : TArgCommonBase<TArgKind::Type>
{
    /** Template argument type.
     */
    Polymorphic<TypeInfo> Type = Polymorphic<TypeInfo>(AutoTypeInfo{});

    auto operator<=>(TypeTArg const&) const = default;
};

} // clang::mrdocs

#endif // MRDOCS_API_METADATA_TARG_TYPETARG_HPP
