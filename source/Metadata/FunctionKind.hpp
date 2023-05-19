//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_METADATA_FUNCTIONKIND_HPP
#define MRDOX_API_METADATA_FUNCTIONKIND_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/FunctionKind.hpp>
#include <llvm/ADT/StringRef.h>
#include <clang/Basic/OperatorKinds.h>

namespace clang {
namespace mrdox {

/** Return the function kind corresponding to clang's enum
*/
FunctionKind
getFunctionKind(
    OverloadedOperatorKind OOK) noexcept;

/** Return a unique string constant for the kind.
*/
llvm::StringRef
getFunctionKindString(
    FunctionKind kind) noexcept;

} // mrdox
} // clang

#endif
