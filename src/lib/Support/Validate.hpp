//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SUPPORT_VALIDATE_HPP
#define MRDOCS_LIB_SUPPORT_VALIDATE_HPP

#include <mrdocs/Platform.hpp>
#include <llvm/ADT/StringRef.h>

/*
    Functions to check the validity of MrDocs output
    to ensure conformance to specifications such
    as Asciidoc section identifiers or XML attributes.
*/

namespace clang {
namespace mrdocs {

/** Return true if s is a valid Asciidoc section ID.
*/
bool
validAdocSectionID(
    llvm::StringRef s) noexcept;

} // mrdocs
} // clang

#endif
