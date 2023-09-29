//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "TestRunner.hpp"
#include "TestArgs.hpp"
#include "lib/Support/Error.hpp"
#include "lib/Support/Path.hpp"
#include "lib/Lib/AbsoluteCompilationDatabase.hpp"
#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Lib/CorpusImpl.hpp"
#include "lib/Lib/SingleFileDB.hpp"
#include "lib/Lib/ToolExecutor.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/Generators.hpp>
#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Support/ThreadPool.hpp>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/Signals.h>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <chrono>

namespace clang {
namespace mrdox {

TestRunner::
TestRunner()
    : diffCmdPath_(llvm::sys::findProgramByName("diff"))
    , xmlGen_(getGenerators().find("xml"))
{
    MRDOX_ASSERT(xmlGen_ != nullptr);
}

Error
TestRunner::
writeFile(
    llvm::StringRef filePath,
    llvm::StringRef contents)
{
    std::error_code ec;
    llvm::raw_fd_ostream os(
        filePath, ec, llvm::sys::fs::OF_None);
    if(ec)
        return Error(ec);
    os << contents;
    if(os.error())
        return Error(os.error());
    return Error::success();
}

void
TestRunner::
handleFile(
    llvm::StringRef filePath,
    std::shared_ptr<ConfigImpl const> config)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    MRDOX_ASSERT(path::extension(filePath).compare_insensitive(".cpp") == 0);

    // Check file-specific config
    if(! config)
    {
        auto configPath = files::withExtension(filePath, "yml");
        auto ft = files::getFileType(configPath);
        if(! ft)
            return report::error("{}: \"{}\"",
                ft.error(), configPath);
        if(ft.value() == files::FileType::regular)
        {
            auto configFile = loadConfigFile(
                configPath,
                    "",
                    "",
                    config,
                    threadPool_);
            if(! configFile)
                return report::error("{}: \"{}\"",
                    configPath, configFile.error());
            config = configFile.value();
        }
        else if(ft.value() != files::FileType::not_found)
        {
            return report::error("{}: \"{}\"",
                Error("not a regular file"), configPath);
        }

        if(! config)
            return report::error("{}: \"{}\"",
                Error("missing config"), filePath);
    }

    SmallPathString dirPath = filePath;
    path::remove_filename(dirPath);

    SmallPathString expectedPath = filePath;
    path::replace_extension(expectedPath, xmlGen_->fileExtension());

    // Build Corpus
    std::unique_ptr<Corpus> corpus;
    {
        SingleFileDB db1(filePath);
        auto workingDir = files::getParentDir(filePath);
        // Convert relative paths to absolute
        AbsoluteCompilationDatabase db(
            llvm::StringRef(workingDir), db1, config);
        ToolExecutor ex(report::Level::debug, *config, db);
        auto result = CorpusImpl::build(ex, config);
        if(! result)
            report::error("{}: \"{}\"", result.error(), filePath);
        corpus = *std::move(result);
    }

    // Generate XML
    std::string generatedXml;
    if(auto err = xmlGen_->buildOneString(generatedXml, *corpus))
        return report::error("{}: \"{}\"", err, filePath);

    // Get expected XML if it exists
    std::unique_ptr<llvm::MemoryBuffer> expectedXml;
    {
        auto fileResult = llvm::MemoryBuffer::getFile(expectedPath, false);
        if(fileResult)
            expectedXml = std::move(fileResult.get());
        else if(fileResult.getError() != std::errc::no_such_file_or_directory)
            return report::error("{}: \"{}\"", fileResult.getError(), expectedPath);
    }

    if(! expectedXml)
    {
        if(testArgs.action == Action::test)
        {
            // Can't test without expected XML file
            return report::error("{}: \"{}\"",
                Error("missing xml file"), expectedPath);
        }

        if(testArgs.action == Action::create)
        {
            // Create expected XML file
            if(auto err = writeFile(expectedPath, generatedXml))
                return report::error("{}: \"{}\"", err, expectedPath);
            report::info("\"{}\" created", expectedPath);
            ++results.expectedXmlWritten;
            return;
        }
    }
    else if(
        testArgs.action == Action::test ||
        testArgs.action == Action::create)
    {
        // Compare the generated output with the expected output
        if(generatedXml != expectedXml->getBuffer())
        {
            // mismatch
            report::error("{}: \"{}\"",
                Error("incorrect results"), filePath);

            if(testArgs.badOption.getValue())
            {
                // Write the .bad.xml file
                auto badPath = expectedPath;
                path::replace_extension(badPath, "bad.xml");
                if(auto err = writeFile(badPath, generatedXml))
                    return report::error("{}: \"{}\"", err, badPath);
                report::info("\"{}\" written", badPath);

                // VFALCO We are calling this over and over again instead of once?
                if(! diffCmdPath_.getError())
                {
                    path::replace_extension(badPath, "xml");
                    std::array<llvm::StringRef, 5u> args {
                        diffCmdPath_.get(), "-u", "--color", expectedPath, badPath };
                    llvm::sys::ExecuteAndWait(diffCmdPath_.get(), args);
                }
            }
        }
        else
        {
            report::info("\"{}\" passed", filePath);
            ++results.expectedXmlMatching;
        }
        return;
    }

    // update action
    if(testArgs.action == Action::update)
    {
        if(! expectedXml || generatedXml != expectedXml->getBuffer())
        {
            // Update the expected XML
            if(auto err = writeFile(expectedPath, generatedXml))
                return report::error("{}: \"{}\"", err, expectedPath);
            report::info("\"{}\" updated", expectedPath);
            ++results.expectedXmlWritten;
        }
        else
        {
            ++results.expectedXmlMatching;
        }
        return;
    }
}

void
TestRunner::
handleDir(
    std::string dirPath,
    std::shared_ptr<ConfigImpl const> config)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    dirPath = files::makeDirsy(dirPath);

    results.numberOfDirs++;

    // Set up directory iterator
    std::error_code ec;
    fs::directory_iterator const end{};
    fs::directory_iterator iter(dirPath, ec, false);
    if(ec)
        return report::error("{}: \"{}\"", dirPath, Error(ec));

    // Check for a directory-wide config
    {
        auto configPath = files::appendPath(dirPath, "mrdox.yml");
        auto ft = files::getFileType(configPath);
        if(! ft)
            return report::error("{}: \"{}\"",
                ft.error(), configPath);
        if(ft.value() == files::FileType::regular)
        {
            auto configFile = loadConfigFile(
                configPath,
                "",
                "",
                config,
                threadPool_);
            if(! configFile)
                return report::error("{}: \"{}\"",
                    configPath, configFile.error());
            config = configFile.value();
        }
        else if(ft.value() != files::FileType::not_found)
        {
            return report::error("{}: \"{}\"",
                Error("not a regular file"), configPath);
        }
    }

    if(! config)
        return report::error("missing configuration: \"{}\"", dirPath);

    // Visit each item in the directory
    while(iter != end)
    {
        if(iter->type() == fs::file_type::directory_file)
        {
            handleDir(iter->path(), config);
        }
        else if(
            iter->type() == fs::file_type::regular_file &&
            path::extension(iter->path()).equals_insensitive(".cpp"))
        {
            threadPool_.async(
                [this, config, filePath = SmallPathString(iter->path())]
                {
                    handleFile(filePath, config);
                });
        }
        else
        {
            // we don't handle this type
        }
        iter.increment(ec);
        if(ec)
            return report::error("{}: \"{}\"",
                Error(ec), dirPath);
    }
}

void
TestRunner::
checkPath(
    std::string inputPath)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    inputPath = files::normalizePath(inputPath);

    // See if inputPath references a file or directory
    auto fileType = files::getFileType(inputPath);
    if(! fileType)
        return report::error("{}: \"{}\"",
            fileType.error(), inputPath);

    switch(fileType.value())
    {
    case files::FileType::regular:
    {
        // Require a .cpp file
        if(! path::extension(inputPath).equals_insensitive(".cpp"))
        {
            Error err("not a .cpp file");
            return report::error("{}: \"{}\"",
                err, inputPath);
        }

        std::shared_ptr<ConfigImpl const> config;

        // Check for a directory-wide config
        {
            auto configPath = files::appendPath(
                files::getParentDir(inputPath), "mrdox.yml");
            auto ft = files::getFileType(configPath);
            if(! ft)
                return report::error("{}: \"{}\"",
                    ft.error(), configPath);
            if(ft.value() == files::FileType::regular)
            {
                auto configFile = loadConfigFile(
                    configPath,
                    "",
                    "",
                    config,
                    threadPool_);
                if(! configFile)
                    return report::error("{}: \"{}\"",
                        configPath, configFile.error());
                config = configFile.value();
            }
            else if(ft.value() != files::FileType::not_found)
            {
                return report::error("{}: \"{}\"",
                    Error("not a regular file"), configPath);
            }
        }

        handleFile(inputPath, config);
        threadPool_.wait();
        return;
    }

    case files::FileType::directory:
    {
        // Iterate this directory and all its children
        handleDir(files::makeDirsy(inputPath), nullptr);
        threadPool_.wait();
        return;
    }

    case files::FileType::not_found:
        return report::error("{}: \"{}\"",
            Error(std::make_error_code(
                std::errc::no_such_file_or_directory)),
            inputPath);

    default:
        return report::error("{}: \"{}\"",
            Error("unknown file type"), inputPath);
    }
}

} // mrdox
} // clang
