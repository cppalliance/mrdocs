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

#ifndef MRDOX_XML_HPP
#define MRDOX_XML_HPP

#include <mrdox/Config.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <string>

namespace clang {
namespace mrdox {

void
renderToXMLString(
    std::string& xml,
    Corpus const& corpus,
    Config const& cfg);

} // mrdox
} // clang

#endif
