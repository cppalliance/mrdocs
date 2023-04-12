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

namespace clang {
namespace mrdox {

Tester::
Tester(
    Config const& config,
    Reporter &R)
    : config_(config)
    , xmlGen(makeXMLGenerator())
    , adocGen(makeAsciidocGenerator())
    , R_(R)
{
}

bool
Tester::
checkDirRecursively(
    llvm::SmallString<340> dirPath,
    llvm::ThreadPool& threadPool)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    std::error_code ec;
    llvm::SmallString<32> outputPath;
    path::remove_dots(dirPath, true);
    fs::directory_iterator const end{};

    fs::directory_iterator iter(dirPath, ec, false);
    if(ec)
    {
        R_.failed("visitDirectory", ec);
        return false;
    }

    while(iter != end)
    {
        if(iter->type() == fs::file_type::directory_file)
        {
            if(! checkDirRecursively(llvm::StringRef(iter->path()), threadPool))
                return false;
        }
        else if(
            iter->type() == fs::file_type::regular_file &&
            path::extension(iter->path()).equals_insensitive(".cpp"))
        {
            outputPath = iter->path();
            path::replace_extension(outputPath, "xml");
            threadPool.async([
                this,
                dirPath,
                inputPath = llvm::SmallString<0>(iter->path()),
                outputPath = llvm::SmallString<340>(outputPath)]() mutable
                {
                    SingleFile db(dirPath, inputPath, outputPath);
                    tooling::StandaloneToolExecutor ex(db, { std::string(inputPath) });
                    auto corpus = Corpus::build(ex, config_, R_);
                    if(corpus)
                        checkOneFile(*corpus, inputPath, outputPath);
                });
        }
        else
        {
            // we don't handle this type
        }
        iter.increment(ec);
        if(ec)
        {
            R_.failed("fs::directory_iterator::increment", ec);
            return false;
        }
    }
    return true;
}

void
Tester::
checkOneFile(
    Corpus& corpus,
    llvm::StringRef inputPath,
    llvm::SmallVectorImpl<char>& outputPathStr)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    llvm::StringRef outputPath;
    outputPath = { outputPathStr.data(), outputPathStr.size() };

    std::string xmlString;
    if(! xmlGen->buildString(xmlString, corpus, config_, R_))
    {
        R_.testFailed();
        return;
    }
    std::error_code ec;
    fs::file_status stat;
    ec = fs::status(outputPathStr, stat, false);
    if(ec == std::errc::no_such_file_or_directory)
    {
        // create the xml file and write to it
        llvm::raw_fd_ostream os(outputPath, ec, llvm::sys::fs::OF_None);
        if(! ec)
        {
            os << xmlString;
        }
        else
        {
            llvm::errs() <<
                "Writing \"" << outputPath << "\" failed: " <<
                ec.message() << "\n";
            R_.testFailed();
        }
    }
    else if(ec)
    {
        R_.failed("fs::status", ec);
        return;
    }
    if(stat.type() != fs::file_type::regular_file)
    {
        R_.failed("'%s' is not a regular file", outputPath);
        return;
    }
    auto bufferResult = llvm::MemoryBuffer::getFile(outputPath, false);
    if(! bufferResult)
    {
        R_.failed("MemoryBuffer::getFile", bufferResult);
        return;
    }
    std::string_view got(bufferResult->get()->getBuffer());
    if(xmlString != bufferResult->get()->getBuffer())
    {
        R_.errs(
            "File: \"", inputPath, "\" failed.\n",
            "Expected:\n",
            bufferResult->get()->getBuffer(), "\n",
            "Got:\n", xmlString, "\n");
        R_.testFailed();
    }

    if(adocGen)
    {
        path::replace_extension(outputPathStr, adocGen->extension());
        outputPath = { outputPathStr.data(), outputPathStr.size() };
        adocGen->buildOne(outputPath, corpus, config_, R_);
    }
}

} // mrdox
} // clang
