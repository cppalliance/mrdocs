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

#include <mrdocs/Metadata/Specifiers/NoexceptInfo.hpp>
#include <format>

namespace mrdocs {

std::string
toString(
    NoexceptInfo const& info,
    bool resolved,
    bool implicit)
{
    if(! implicit && info.Implicit)
        return "";
    if(info.Kind == NoexceptKind::Dependent &&
        info.Operand.empty())
        return "";
    if(info.Kind == NoexceptKind::False &&
        (resolved || info.Operand.empty()))
        return "";
    if(info.Kind == NoexceptKind::True &&
        (resolved || info.Operand.empty()))
        return "noexcept";
    return std::format("noexcept({})", info.Operand);
}

} // mrdocs
