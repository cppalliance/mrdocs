//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_ADOC_ADOCESCAPE_HPP
#define MRDOCS_LIB_GEN_ADOC_ADOCESCAPE_HPP

#include <mrdocs/Support/Handlebars.hpp>
#include <string>
#include <string_view>

namespace clang::mrdocs::adoc {

/** Escape a string for use in AsciiDoc
  */
MRDOCS_DECL
void
AdocEscape(OutputRef& os, std::string_view str);

MRDOCS_DECL
std::string
AdocEscape(std::string_view str);

} // clang::mrdocs::adoc

#endif
