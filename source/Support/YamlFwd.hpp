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

#ifndef MRDOX_TOOL_SUPPORT_YAMLFWD_HPP
#define MRDOX_TOOL_SUPPORT_YAMLFWD_HPP

#include <mrdox/Platform.hpp>

namespace llvm {
namespace yaml {

template<class T>
struct MappingTraits;

} // yaml

class SMDiagnostic;

} // llvm

#endif
