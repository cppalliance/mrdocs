//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Specifiers/ExplicitInfo.hpp>
#include <format>

namespace mrdocs {

std::string
toString(
    ExplicitInfo const& info,
    bool resolved,
    bool implicit)
{
    if(! implicit && info.Implicit)
        return "";
    if(info.Kind == ExplicitKind::Dependent &&
        info.Operand.empty())
        return "";
    if(info.Kind == ExplicitKind::False &&
        (resolved || info.Operand.empty()))
        return "";
    if(info.Kind == ExplicitKind::True &&
        (resolved || info.Operand.empty()))
        return "explicit";
    return std::format("explicit({})", info.Operand);
}

} // mrdocs
