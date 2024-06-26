//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "TestRunner.hpp"
#include "TestArgs.hpp"
#include "lib/Support/Error.hpp"
#include "lib/Support/Path.hpp"
#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Lib/CorpusImpl.hpp"
#include "lib/Lib/MrDocsCompilationDatabase.hpp"
#include "lib/Lib/SingleFileDB.hpp"
#include "test_suite/diff.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Generators.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
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
namespace mrdocs {

TestRunner::
TestRunner()
    : diffCmdPath_(llvm::sys::findProgramByName("diff"))
    , xmlGen_(getGenerators().find("xml"))
{
    MRDOCS_ASSERT(xmlGen_ != nullptr);
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
    fmt::println("handleFile: {}", filePath);

    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    MRDOCS_ASSERT(path::extension(filePath).compare_insensitive(".cpp") == 0);

    // Check file-specific config
    auto configPath = files::withExtension(filePath, "yml");
    auto ft = files::getFileType(configPath);
    if(! ft)
        return report::error("{}: \"{}\"",
            ft.error(), configPath);

    // if(! config)
    if (ft.value() != files::FileType::not_found)
    {
        if(ft.value() != files::FileType::regular)
            return report::error("{}: \"{}\"",
                Error("not a regular file"), configPath);
        Config::Settings publicSettings = config->settings();
        publicSettings.config = configPath;
        publicSettings.configDir = files::getParentDir(filePath);
        publicSettings.configYaml = files::getFileText(publicSettings.config).value();
        loadConfig(publicSettings, publicSettings.configYaml).value();
        auto configFile = loadConfigFile(
            publicSettings,
            config,
            threadPool_);
        if(! configFile)
            return report::error("{}: \"{}\"",
                configPath, configFile.error());
        config = configFile.value();
    }
    if(! config)
        return report::error("{}: \"{}\"",
            Error("missing config"), filePath);

    SmallPathString dirPath = filePath;
    path::remove_filename(dirPath);

    SmallPathString expectedPath = filePath;
    path::replace_extension(expectedPath, xmlGen_->fileExtension());

    auto parentDir = files::getParentDir(filePath);

    std::unordered_map<std::string, std::vector<std::string>> defaultIncludePaths;

    // Convert relative paths to absolute
    MrDocsCompilationDatabase compilations(
        llvm::StringRef(parentDir), SingleFileDB(filePath), config, defaultIncludePaths);
    // Build Corpus
    auto corpus = CorpusImpl::build(
        report::Level::debug, config, compilations);
    if(! corpus)
        return report::error("{}: \"{}\"", corpus.error(), filePath);

    // Generate XML
    std::string generatedXml;
    if(auto err = xmlGen_->buildOneString(generatedXml, **corpus))
        return report::error("{}: \"{}\"", err, filePath);

    // Get expected XML if it exists
    std::unique_ptr<llvm::MemoryBuffer> expectedXml;
    {
        auto fileResult = llvm::MemoryBuffer::getFile(expectedPath, false, true, true);
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
            auto res = test_suite::diffStrings(
                expectedXml->getBuffer(), generatedXml);
            report::error("{} lines added", res.added);
            report::error("{} lines removed", res.removed);
            report::error("Diff:\n{}", res.diff);

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

namespace {
    Expected<void>
    loadTestDirConfig(
        std::string const& dirPath,
        std::shared_ptr<ConfigImpl const>& config,
        ThreadPool& threadPool)
    {
        namespace fs = llvm::sys::fs;
        namespace path = llvm::sys::path;

        auto configPath = files::appendPath(dirPath, "mrdocs.yml");
        auto ft = files::getFileType(configPath);
        if(! ft)
        {
            report::error("{}: \"{}\"", ft.error(), configPath);
            return ft.error();
        }
        if(ft.value() == files::FileType::regular)
        {
            Config::Settings publicSettings;
            publicSettings.config = configPath;
            publicSettings.configDir = dirPath;
            publicSettings.configYaml = files::getFileText(publicSettings.config).value();
            loadConfig(publicSettings, publicSettings.configYaml).value();
            publicSettings.sourceRoot = files::makeAbsolute(
                publicSettings.sourceRoot,
                publicSettings.configDir);
            auto configFile = loadConfigFile(
                publicSettings,
                config,
                threadPool);
            if(! configFile)
            {
                report::error("{}: \"{}\"", configPath, configFile.error());
                return configFile.error();
            }

            config = configFile.value();
        }
        else if(ft.value() != files::FileType::not_found)
        {
            report::error("{}: \"{}\"", Error("not a regular file"), configPath);
            return Error("not a regular file");
        }
        return {};
    }
}

void
TestRunner::
handleDir(
    std::string dirPath,
    std::shared_ptr<ConfigImpl const> config)
{
    fmt::println("handleDir: {}", dirPath);

    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    dirPath = files::makeDirsy(dirPath);

    results.numberOfDirs++;

    // Load a directory-wide config file
    if (auto e = loadTestDirConfig(dirPath, config, threadPool_); !e)
        return;
    if(! config)
        return report::error("missing configuration: \"{}\"", dirPath);

    // Visit each file in the directory
    std::error_code ec;
    fs::directory_iterator const end{};
    fs::directory_iterator iter(dirPath, ec, false);
    if(ec)
        return report::error("{}: \"{}\"", dirPath, Error(ec));
    while(iter != end)
    {
        auto const& entry = *iter;
        if(entry.type() == fs::file_type::directory_file)
        {
            handleDir(entry.path(), config);
        }
        else if(
            entry.type() == fs::file_type::regular_file &&
            path::extension(entry.path()).equals_insensitive(".cpp"))
        {
            threadPool_.async(
                [this, config, filePath = SmallPathString(entry.path())]
                {
                    handleFile(filePath, config);
                });
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
                files::getParentDir(inputPath), "mrdocs.yml");
            auto ft = files::getFileType(configPath);
            if(! ft)
                return report::error("{}: \"{}\"",
                    ft.error(), configPath);
            if(ft.value() == files::FileType::regular)
            {
                Config::Settings publicSettings;
                publicSettings.config = configPath;
                publicSettings.configDir = files::getParentDir(inputPath);
                publicSettings.configYaml = files::getFileText(publicSettings.config).value();
                if (testArgs.addons.getValue() != "")
                {
                    publicSettings.addons = files::normalizeDir(testArgs.addons.getValue());
                }
                else
                {
                    report::warn("No addons directory specified to mrdocs tests");
                }
                loadConfig(publicSettings, publicSettings.configYaml).value();
                auto configFile = loadConfigFile(
                    publicSettings,
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

} // mrdocs
} // clang
