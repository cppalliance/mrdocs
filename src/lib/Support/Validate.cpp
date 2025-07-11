//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2025 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Validate.hpp"
#include <cctype>

namespace clang {
namespace mrdocs {

bool
validAdocSectionID(
    llvm::StringRef s) noexcept
{
    // can't be empty
    if(s.empty())
        return false;

    // https://docs.asciidoctor.org/asciidoc/latest/sections/custom-ids/
    //
    // first char must be alpha, '_', or ':'
    //
    return
        s[0] == '_' ||
        s[0] == ':' ||
        std::isalpha(static_cast<unsigned char>(s[0]));
}

} // mrdocs
} // clang
