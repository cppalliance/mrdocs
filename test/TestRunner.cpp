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
#include "Support/Error.hpp"
#include "Support/Path.hpp"
#include "Tool/AbsoluteCompilationDatabase.hpp"
#include "Tool/ConfigImpl.hpp"
#include "Tool/CorpusImpl.hpp"
#include "Tool/SingleFileDB.hpp"
#include "Tool/ToolExecutor.hpp"
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
    : threadPool_(1)
    , diffCmdPath_(llvm::sys::findProgramByName("diff"))
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
    llvm::raw_fd_ostream os(filePath, ec, llvm::sys::fs::OF_None);
    if(ec)
    {
        results.numberOfErrors++;
        return formatError("raw_fd_ostream(\"{}\") returned \"{}\"", filePath, ec);
    }
    os << contents;
    if(os.error())
    {
        results.numberOfErrors++;
        return formatError("raw_fd_ostream::write returned \"{}\"", os.error());
    }
    results.numberofFilesWritten++;
    return Error::success();
}

Error
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
            return ft.error();
        if(ft.value() == files::FileType::regular)
        {
            auto result = loadConfigFile(
                configPath, testArgs.addonsDir, "", config);
            if(! result)
            {
                results.numberOfErrors++;
                reportWarning("Error loading \"{}\": {}", filePath, result.error());
                return Error::success();
            }
            config = result.value();
        }

        if(! config)
        {
            reportWarning(fmt::format("Error: \"{}\", missing configuration file", filePath));
            return Error::success();
        }
    }

    results.numberOfFiles++;

    SmallPathString dirPath = filePath;
    path::remove_filename(dirPath);

    SmallPathString outputPath = filePath;
    path::replace_extension(outputPath, xmlGen_->fileExtension());

    // Build Corpus
    std::unique_ptr<Corpus> corpus;
    {
        SingleFileDB db1(filePath);
        auto workingDir = files::getParentDir(filePath);
        // Convert relative paths to absolute
        AbsoluteCompilationDatabase db(
            llvm::StringRef(workingDir), db1, config);
        ToolExecutor ex(*config, db);
        auto result = CorpusImpl::build(ex, config);
        if(! result)
        {
            reportError(result.error(), "build Corpus for \"{}\"", filePath);
            return Error::success(); // keep going
        }
        corpus = result.release();
    }

    // Generate XML
    std::string generatedXml;
    if(auto err = xmlGen_->buildOneString(generatedXml, *corpus))
    {
        reportError(err, "build XML string for \"{}\"", filePath);
        results.numberOfErrors++;
        return Error::success(); // keep going
    }

    if(testArgs.action == Action::test)
    {
        // Open and load XML comparison file
        std::unique_ptr<llvm::MemoryBuffer> expectedXml;
        {
            auto result = llvm::MemoryBuffer::getFile(outputPath, false);
            if(! result)
            {
                // File could not be loaded
                results.numberOfErrors++;

                if( result.getError() != std::errc::no_such_file_or_directory)
                {
                    // Some kind of system problem
                    reportError(
                        Error("MemoryBuffer::getFile(\"{}\") returned \"{}\""),
                        "load the reference XML");
                    return Error::success(); // keep going
                }

                // File does not exist, so write it
                if(auto err = writeFile(outputPath, generatedXml))
                    return Error::success();
            }
            else
            {
                expectedXml = std::move(result.get());
            }
        }

        // Compare the generated output with the expected output
        if( expectedXml &&
            generatedXml != expectedXml->getBuffer())
        {
            // The output did not match
            results.numberOfFailures++;
            reportError("Test for \"{}\" failed", filePath);

            if(testArgs.badOption.getValue())
            {
                // Write the .bad.xml file
                auto bad = outputPath;
                path::replace_extension(bad, "bad.xml");
                {
                    std::error_code ec;
                    llvm::raw_fd_ostream os(bad, ec, llvm::sys::fs::OF_None);
                    if (ec)
                    {
                        results.numberOfErrors++;
                        return formatError("raw_fd_ostream(\"{}\") returned \"{}\"", bad, ec);
                    }
                    os << generatedXml;
                }

                // VFALCO We are calling this over and over again instead of once?
                if(! diffCmdPath_.getError())
                {
                    path::replace_extension(bad, "xml");
                    std::array<llvm::StringRef, 5u> args {
                        diffCmdPath_.get(), "-u", "--color", outputPath, bad };
                    llvm::sys::ExecuteAndWait(diffCmdPath_.get(), args);
                }

                // Fix the path for the code that follows
                outputPath.pop_back_n(8);
                path::replace_extension(outputPath, "xml");
            }
        }
        else
        {
            // success
        }
    }
    else if(testArgs.action == Action::update)
    {
        // Refresh the expected output file
        if(auto err = writeFile(outputPath, generatedXml))
        {
            results.numberOfErrors++;
            return Error::success();
        }
    }

    return Error::success();
}

Error
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
    fs::directory_iterator iter(dirPath, ec, false);
    if(ec)
        return Error(ec);
    fs::directory_iterator const end{};

    // Check directory-wide config
    {
        auto configPath = files::appendPath(dirPath, "mrdox.yml");
        auto ft = files::getFileType(configPath);
        if(! ft)
            return ft.error();
        if(ft.value() == files::FileType::regular)
        {
            auto result = loadConfigFile(
                configPath, testArgs.addonsDir, "", config);
            if(! result)
                return result.error();
            config = result.value();
        }
    }

    while(iter != end)
    {
        if(iter->type() == fs::file_type::directory_file)
        {
            if(auto err = handleDir(iter->path(), config))
                return err;
        }
        else if(
            iter->type() == fs::file_type::regular_file &&
            path::extension(iter->path()).equals_insensitive(".cpp"))
        {
            threadPool_.async(
                [this, config, filePath = SmallPathString(iter->path())]
                {
                    handleFile(filePath, config).operator bool();
                });
        }
        else
        {
            // we don't handle this type
        }
        iter.increment(ec);
        if(ec)
            return formatError("directory_iterator::increment returned \"{}\"", ec);
    }
    return Error::success();
}

Error
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
        return fileType.error();

    switch(fileType.value())
    {
    case files::FileType::regular:
    {
        auto const ext = path::extension(inputPath);
        if(! ext.equals_insensitive(".cpp"))
            return formatError("\"{}\" is not a .cpp file");

        std::shared_ptr<ConfigImpl const> config;

        // Check directory-wide config
        {
            auto configPath = files::appendPath(
                files::getParentDir(inputPath), "mrdox.yml");
            auto ft = files::getFileType(configPath);
            if(ft && ft.value() == files::FileType::regular)
            {
                auto result = loadConfigFile(
                    configPath, testArgs.addonsDir);
                if(! result)
                    return result.error();
                config = result.value();
            }
        }

        auto err = handleFile(inputPath, config);
        threadPool_.wait();
        return err;
    }

    case files::FileType::directory:
    {
        // Iterate this directory and all its children
        auto err = handleDir(files::makeDirsy(inputPath), nullptr);
        threadPool_.wait();
        return err;
    }

    case files::FileType::not_found:
        return Error(std::make_error_code(
            std::errc::no_such_file_or_directory));

    default:
        return Error("unknown FileType");
    }
}

} // mrdox
} // clang
