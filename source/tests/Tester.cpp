//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Tester.hpp"
#include "SingleFile.hpp"
#include <clang/Tooling/StandaloneExecution.h>

//#define NO_ASYNC

namespace clang {
namespace mrdox {

Tester::
Tester(
    std::shared_ptr<Config> const& config,
    Reporter &R)
    : config_(config)
    , xmlGen(makeXMLGenerator())
    , adocGen(makeAsciidocGenerator())
    , R_(R)
{
}

void
Tester::
checkDirRecursively(
    llvm::SmallString<340> dirPath,
    Config::WorkGroup& workGroup)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    std::error_code ec;
    llvm::SmallString<32> outputPath;
    path::remove_dots(dirPath, true);
    fs::directory_iterator const end{};
    fs::directory_iterator iter(dirPath, ec, false);
    if(R_.error(ec, "iterate the directory '", dirPath, '.'))
        return;

    while(iter != end)
    {
        if(iter->type() == fs::file_type::directory_file)
        {
            checkDirRecursively(
                llvm::StringRef(iter->path()), workGroup);
        }
        else if(
            iter->type() == fs::file_type::regular_file &&
            path::extension(iter->path()).equals_insensitive(".cpp"))
        {
            outputPath = iter->path();
            path::replace_extension(outputPath, "");
            workGroup.post(
                [
                this,
                dirPath,
                inputPath = llvm::SmallString<0>(iter->path()),
                outputPath = llvm::SmallString<340>(outputPath)
                    ]() mutable
                {
                    SingleFile db(dirPath, inputPath, outputPath);
                    tooling::StandaloneToolExecutor ex(db, { std::string(inputPath) });
                    auto corpus = Corpus::build(ex, config_, R_);
                    if(! R_.error(corpus, "build corpus for '", inputPath, "'"))
                        checkOneFile(**corpus, inputPath, outputPath);
                });
        }
        else
        {
            // we don't handle this type
        }
        iter.increment(ec);
        if(R_.error(ec, "iterate the directory '", dirPath, '.'))
            return;
    }
}

void
Tester::
checkOneFile(
    Corpus const& corpus,
    llvm::StringRef inputPath,
    llvm::SmallString<340>& outputPath)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // build XML
    std::string xmlString;
    if(! xmlGen->buildString(xmlString, corpus, R_))
        return;

    // check that the XML file exists and is valid
    std::error_code ec;
    fs::file_status stat;
    path::replace_extension(outputPath, "xml");
    ec = fs::status(outputPath, stat, false);
    if(ec == std::errc::no_such_file_or_directory)
    {
        // If the file doesn't already exist, then
        // create it and write the produced XML.
        // This counts as a test failure.
        R_.reportTestFailure();
        llvm::raw_fd_ostream os(outputPath, ec, llvm::sys::fs::OF_None);
        if(R_.error(ec, "open the file '", outputPath, "' for writing"))
            return;
        os << xmlString;
        if(R_.error(os.error(), "write the file '", outputPath, "'"))
            return;
        R_.reportError();
        R_.print("Wrote: file '", outputPath, "'");
        return;
    }
    if(R_.error(ec, "call fs::status on '", outputPath, "'"))
    {
        return;
    }
    if(stat.type() != fs::file_type::regular_file)
    {
        R_.failed("Couldn't open '", outputPath, "' because it is not a regular file");
        return;
    }
    {
        // read the XML file to get the expected output
        auto expectedXml = llvm::MemoryBuffer::getFile(outputPath, false);
        if(R_.error(expectedXml, "read the file '", outputPath, "'"))
            return;
        if(xmlString != expectedXml->get()->getBuffer())
        {
            // The output did not match, write the
            // mismatched XML to a .bad.xml file
            llvm::SmallString<340> badPath = outputPath;
            path::replace_extension(badPath, ".bad.xml");
            std::error_code ec;
            llvm::raw_fd_ostream os(badPath, ec, llvm::sys::fs::OF_None);
            if(! R_.error(ec, "open '", badPath, "' for writing"))
                os << xmlString;
            R_.print("Failed: \"", inputPath, "\"\n");
            R_.reportTestFailure();
        }
    }

    if(adocGen)
    {
        path::replace_extension(outputPath, adocGen->extension());
        adocGen->buildOne(outputPath, corpus, R_);
    }
}

} // mrdox
} // clang
