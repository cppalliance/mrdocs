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

#ifndef MRDOCS_API_METADATA_TPARAM_TPARAMKIND_HPP
#define MRDOCS_API_METADATA_TPARAM_TPARAMKIND_HPP

#include <mrdocs/Platform.hpp>
#include <string_view>

namespace clang::mrdocs {

enum class TParamKind : int
{
    #define INFO(Type) Type,
    #include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>
};

MRDOCS_DECL
std::string_view
toString(TParamKind kind) noexcept;

} // clang::mrdocs

#endif
