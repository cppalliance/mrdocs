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

#ifndef MRDOCS_API_METADATA_TARG_TEMPLATETARG_HPP
#define MRDOCS_API_METADATA_TARG_TEMPLATETARG_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/TArg/ConstantTArg.hpp>
#include <mrdocs/Metadata/TArg/TArgBase.hpp>
#include <mrdocs/Metadata/TArg/TypeTArg.hpp>

namespace clang::mrdocs {

struct TemplateTArg final
    : TArgCommonBase<TArgKind::Template>
{
    /** SymbolID of the referenced template. */
    SymbolID Template;

    /** Name of the referenced template. */
    std::string Name;

    auto operator<=>(TemplateTArg const&) const = default;
};

} // clang::mrdocs

#endif // MRDOCS_API_METADATA_TARG_TEMPLATETARG_HPP
