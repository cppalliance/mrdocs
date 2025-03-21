//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "XMLGenerator.hpp"
#include "XMLWriter.hpp"
#include "lib/Support/Radix.hpp"
#include "lib/Support/RawOstream.hpp"
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Metadata.hpp>

namespace clang {
namespace mrdocs {
namespace xml {

Expected<void>
XMLGenerator::
buildOne(
    std::ostream& os,
    Corpus const& corpus) const
{
    namespace fs = llvm::sys::fs;
    RawOstream raw_os(os);
    return XMLWriter(raw_os, corpus).build();
}

} // xml

//------------------------------------------------

std::unique_ptr<Generator>
makeXMLGenerator()
{
    return std::make_unique<xml::XMLGenerator>();
}

} // mrdocs
} // clang
