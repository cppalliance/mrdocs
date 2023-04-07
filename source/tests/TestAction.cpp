//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "TestAction.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/XML.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

void
TestAction::
EndSourceFileAction()
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    auto rv = buildCorpus(ex_, cfg_, R_);
    if(! rv)
        return;
    std::string xml;
    renderToXMLString(xml, *rv, cfg_);
    llvm::SmallString<256> xmlPath(this->getCurrentFile());
    path::replace_extension(xmlPath, "xml");
    std::error_code ec;
    fs::file_status stat;
    ec = fs::status(xmlPath, stat, false);
    if(ec == std::errc::no_such_file_or_directory)
    {
        // create the xml file and write to it
    }
    else if(R_.success(ec))
    {
        if(stat.type() == fs::file_type::regular_file)
        {
            std::unique_ptr<llvm::MemoryBuffer> pb;
            if(! R_.success(pb, llvm::MemoryBuffer::getFile(xmlPath, true)))
                return;
            if(xml != pb->getBuffer())
            {
                llvm::errs() <<
                    "File: \"" << this->getCurrentFile() << "\" failed.\n"
                    "Expected:\n" <<
                    pb->getBuffer() << "\n" <<
                    "Got:\n" <<
                    xml << "\n";
                R_.test_failure();
            }
        }
        else
        {
            // VFALCO report that it is not a regular file
        }
    }
}

} // mrdox
} // clang
