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
    if(R_.error(ec, "iterate the directory '", dirPath, '.'))
        return false;

    while(iter != end)
    {
        if(iter->type() == fs::file_type::directory_file)
        {
            checkDirRecursively(llvm::StringRef(iter->path()), threadPool);
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
            return false;
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
        return;
    std::error_code ec;
    fs::file_status stat;
    ec = fs::status(outputPathStr, stat, false);
    if(ec == std::errc::no_such_file_or_directory)
    {
        // create the xml file and write to it
        R_.reportTestFailure();
        llvm::raw_fd_ostream os(outputPath, ec, llvm::sys::fs::OF_Text);
        if(R_.error(ec, "open the file '", outputPath, "' for writing"))
            return;
        os << xmlString;
        if(R_.error(os.error(), "write the file '", outputPath, "'"))
            return;
        R_.reportError();
        // keep going, to write the other files
    }
    else if(R_.error(ec, "call fs::status on '", outputPathStr, "'"))
    {
        return;
    }
    else if(stat.type() != fs::file_type::regular_file)
    {
        R_.failed("Couldn't open '", outputPath, "' because it is not a regular file");
        return;
    }
    else
    {
        // perform test
        auto bufferResult = llvm::MemoryBuffer::getFile(outputPath, false);
        if(R_.error(bufferResult, "read the file '", outputPath, "'"))
            return;
        std::string_view good(bufferResult->get()->getBuffer());
        if(xmlString != good)
        {
            R_.print(
                "File: \"", inputPath, "\" failed.\n",
                "Expected:\n",
                good, "\n",
                "Got:\n", xmlString, "\n");
            R_.reportTestFailure();
        }
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
