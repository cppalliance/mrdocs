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

#include "XMLGenerator.hpp"
#include "XMLWriter.hpp"
#include "api/Support/Radix.hpp"
#include "api/Support/RawOstream.hpp"
#include <mrdox/Error.hpp>
#include <mrdox/Metadata.hpp>

namespace clang {
namespace mrdox {
namespace xml {

Err
XMLGenerator::
buildOne(
    std::ostream& os,
    Corpus const& corpus,
    Reporter& R) const
{
    namespace fs = llvm::sys::fs;
    RawOstream raw_os(os);
    return XMLWriter(raw_os, corpus, R).build();
}

} // xml

//------------------------------------------------

std::unique_ptr<Generator>
makeXMLGenerator()
{
    return std::make_unique<xml::XMLGenerator>();
}

} // mrdox
} // clang
