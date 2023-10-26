//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZE_HPP
#define MRDOCS_LIB_METADATA_FINALIZE_HPP

#include "lib/Lib/Info.hpp"

namespace clang {
namespace mrdocs {

void finalize(InfoSet& Info);

} // mrdocs
} // clang

#endif
