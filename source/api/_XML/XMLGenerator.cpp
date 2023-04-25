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
#include "Support/Radix.hpp"
#include <mrdox/Error.hpp>
#include <mrdox/Metadata.hpp>

namespace clang {
namespace mrdox {
namespace xml {

llvm::Error
XMLGenerator::
buildSinglePage(
    llvm::raw_ostream& os,
    Corpus const& corpus,
    Reporter& R,
    llvm::raw_fd_ostream* fd_os) const
{
    namespace fs = llvm::sys::fs;
    XMLWriter w(os, fd_os, corpus, R);
    return w.build();
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
