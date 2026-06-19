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
#include <lib/Support/Generator.hpp>
#include <lib/Support/Radix.hpp>
#include <lib/Support/RawOstream.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Error.hpp>


namespace mrdocs {
namespace xml {

Expected<void>
XMLGenerator::
build(Corpus const& corpus) const
{
    // The XML generator emits a single file. Resolve the output location
    // from the config and write the document into it (or into
    // reference.xml when the output names a directory).
    std::string const out = getGeneratorOutputPath(*this, corpus);
    MRDOCS_TRY(std::string const fileName,
        getSinglePageFullPath(out, fileExtension()));
    return writeToFile(fileName, [&](std::ostream& os) -> Expected<void>
    {
        RawOstream raw_os(os);
        return XMLWriter(raw_os, corpus).build();
    });
}

} // xml

//------------------------------------------------

std::unique_ptr<Generator>
makeXMLGenerator()
{
    return std::make_unique<xml::XMLGenerator>();
}

} // mrdocs

