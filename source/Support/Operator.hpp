//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_SUPPORT_OPERATOR_HPP
#define MRDOX_TOOL_SUPPORT_OPERATOR_HPP

#include <mrdox/Platform.hpp>
#include <clang/Basic/OperatorKinds.h>
#include <llvm/ADT/StringRef.h>

namespace clang {
namespace mrdox {

llvm::StringRef
getSafeOperatorName(
    OverloadedOperatorKind OOK) noexcept;

} // mrdox
} // clang

#endif
