//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "ClangDoc.h"
#include "Representation.h"
#include <mrdox/ClangDocContext.hpp>
#include <clang/tooling/Tooling.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>
#include <cstdlib>

// Each test comes as a pair of files.
// A .cpp file containing valid declarations,
// and a .xml file containing the expected output
// of the XML generator, which must match exactly.

namespace clang {
namespace mrdox {

namespace {

//------------------------------------------------

int
do_main(int argc, const char** argv)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    llvm::sys::PrintStackTraceOnErrorSignal(argv[0]);

    ClangDocContext CDCtx;
    if(llvm::Error err = setupContext(CDCtx, argc, argv))
    {
        llvm::errs() << "test failure: " << err << "\n";
        return EXIT_FAILURE;
    }

    for(int i = 1; i < argc; ++i)
    {
        std::error_code ec;
        llvm::SmallString<256> dir(argv[i]);
        path::remove_dots(dir, true);
        fs::recursive_directory_iterator iter(dir, ec, false);
        if(ec)
        {
            llvm::errs() <<
                ec.message() << ": \"" <<
                dir << "\"\n";
            return EXIT_FAILURE;
        }
        auto const& name = iter->path();
        llvm::SmallString<128> cppPath(name);
        llvm::SmallString<128> xmlPath(name);
        path::remove_dots(cppPath, true);
        path::remove_dots(xmlPath, true);
        if(path::extension(name).equals_insensitive(".cpp"))
        {
            if(! fs::is_regular_file(name))
            {
                llvm::errs() <<
                    "invalid file: \"" << name << "\"\n";
                return EXIT_FAILURE;
            }
            path::replace_extension(xmlPath, "xml");
            if( ! fs::exists(xmlPath) ||
                ! fs::is_regular_file(xmlPath))
            {
                llvm::errs() <<
                    "missing or invalid file: \"" << xmlPath << "\"\n";
                return EXIT_FAILURE;
            }
        }
        else if(path::extension(name).equals_insensitive(".xml"))
        {
            path::replace_extension(cppPath, "cpp");
            if( ! fs::exists(cppPath) ||
                ! fs::is_regular_file(cppPath))
            {
                llvm::errs() <<
                    "missing or invalid file: \"" << cppPath << "\"\n";
                return EXIT_FAILURE;
            }

            // don't process the same test twice
            continue;
        }

        llvm::StringRef cppCode;
        auto cppResult = llvm::MemoryBuffer::getFile(cppPath, true);
        if(! cppResult)
        {
            llvm::errs() <<
                cppResult.getError().message() << ": \"" << xmlPath << "\"\n";
            return EXIT_FAILURE;
        }
        cppCode = cppResult->get()->getBuffer();

        llvm::StringRef expectedXml;
        auto xmlResult = llvm::MemoryBuffer::getFile(xmlPath, true);
        if(! xmlResult)
        {
            llvm::errs() <<
                xmlResult.getError().message() << ": \"" << xmlPath << "\"\n";
            return EXIT_FAILURE;
        }
        expectedXml = xmlResult->get()->getBuffer();

        bool success = clang::tooling::runToolOnCode(
            makeFrontendAction(CDCtx), cppCode, cppPath);
        if(! success)
            llvm::errs() <<
                "Frontend action failed\n";
    }

    return EXIT_SUCCESS;
}

} // (anon)

} // mrdox
} // clang

int
main(int argc, const char** argv)
{
    return clang::mrdox::do_main(argc, argv);
}
