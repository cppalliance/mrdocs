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
#include "lib/Gen/hbs/HandlebarsGenerator.hpp"
#include "test_suite/diff.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Generators.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Error.hpp>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/Program.h>
#include <atomic>
#include <iostream>

namespace clang::mrdocs {

TestRunner::
TestRunner(std::string_view generator)
    : diffCmdPath_(llvm::sys::findProgramByName("diff"))
    , gen_(getGenerators().find(generator))
{
    MRDOCS_ASSERT(gen_ != nullptr);
}

Expected<void>
TestRunner::
writeFile(
    llvm::StringRef filePath,
    llvm::StringRef contents)
{
    std::error_code ec;
    llvm::raw_fd_ostream os(
        filePath, ec, llvm::sys::fs::OF_None);
    MRDOCS_CHECK(!ec, ec);
    os << contents;
    MRDOCS_CHECK(!os.has_error(), os.error());
    return {};
}

void
replaceCRLFWithLF(std::string &str)
{
    std::string::size_type pos = 0;
    while ((pos = str.find("\r\n", pos)) != std::string::npos) {
        str.replace(pos, 2, "\n");
        pos += 1; // Move past the '\n' character
    }
}

void
TestRunner::
handleFile(
    llvm::StringRef filePath,
    Config::Settings const& dirSettings)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    MRDOCS_ASSERT(path::extension(filePath).compare_insensitive(".cpp") == 0);

    // Check the source file
    auto ft = files::getFileType(filePath);
    if (ft.value() == files::FileType::not_found) {
        return report::error("{}: \"{}\"",
            Error("file not found"), filePath);
    }
    if(ft.value() != files::FileType::regular) {
        return report::error("{}: \"{}\"",
             Error("not a regular file"), filePath);
    }

    // File-specific config
    Config::Settings fileSettings = dirSettings;
    auto configPath = files::withExtension(filePath, "yml");
    if (files::exists(configPath)) {
        if (auto exp = Config::Settings::load_file(fileSettings, configPath, dirs_); !exp)
        {
            return report::error("Failed to load config file: {}: \"{}\"", exp.error(), configPath);
        }
        if (auto exp = fileSettings.normalize(dirs_); !exp)
        {
            return report::error("Failed to normalize config file: {}: \"{}\"", exp.error(), configPath);
        }
    }

    // Config Implementation
    std::shared_ptr<ConfigImpl const> config =
        ConfigImpl::load(fileSettings, dirs_, threadPool_).value();

    // Path with the expected results
    SmallPathString expectedPath = filePath;
    path::replace_extension(expectedPath, gen_->fileExtension());

    // Create an adjusted MrDocsDatabase
    auto parentDir = files::getParentDir(filePath);
    std::unordered_map<std::string, std::vector<std::string>> defaultIncludePaths;
    MrDocsCompilationDatabase compilations(
        llvm::StringRef(parentDir), SingleFileDB(filePath), config, defaultIncludePaths);

    report::setMinimumLevel(report::Level::error);

    // Build Corpus
    auto corpus = CorpusImpl::build(config, compilations);
    if (!corpus)
    {
        return report::error("{}: \"{}\"", corpus.error(), filePath);
    }

    // Generate
    std::string generatedDocs;
    if (auto exp = gen_->buildOneString(generatedDocs, **corpus); !exp)
    {
        return report::error("{}: \"{}\"", exp.error(), filePath);
    }
    replaceCRLFWithLF(generatedDocs);

    // Generate tagfile
    if (auto hbsGen = dynamic_cast<hbs::HandlebarsGenerator const*>(gen_))
    {
        std::stringstream ss;
        if (auto exp = hbsGen->buildTagfile(ss, **corpus); !exp)
        {
            return report::error("{}: \"{}\"", exp.error(), filePath);
        }
    }

    // Get expected documentation if it exists
    std::unique_ptr<llvm::MemoryBuffer> expectedDocsBuf;
    {
        auto fileResult = llvm::MemoryBuffer::getFile(expectedPath, false, true, true);
        if (fileResult)
        {
            expectedDocsBuf = std::move(fileResult.get());
        } else if (fileResult.getError() != std::errc::no_such_file_or_directory)
        {
            return report::
                error("{}: \"{}\"", fileResult.getError(), expectedPath);
        }
    }

    // If no expected documentation file
    if(!expectedDocsBuf)
    {
        if(testArgs.action == Action::test)
        {
            // Can't test without expected documentation file
            return report::error("{}: \"{}\"",
                Error("missing test file"), expectedPath);
        }

        if(testArgs.action == Action::create ||
           testArgs.action == Action::update)
        {
            // Create expected documentation file
            if(auto exp = writeFile(expectedPath, generatedDocs);
                !exp)
            {
                return report::error("{}: \"{}\"", exp.error(), expectedPath);
            }
            report::info("\"{}\" created", expectedPath);
            ++results.expectedDocsWritten;
            return;
        }
    }

    // Analyse results
    std::string expectedDocs = expectedDocsBuf->getBuffer().str();
    replaceCRLFWithLF(expectedDocs);
    if (generatedDocs == expectedDocs)
    {
        report::info("\"{}\" passed", filePath);
        ++results.expectedDocsMatching;
        return;
    }

    // Mismatch
    if(
        testArgs.action == Action::test ||
        testArgs.action == Action::create)
    {
        std::string_view filePathSv = filePath;
        if (filePathSv.starts_with(dirs_.cwd))
        {
            filePathSv.remove_prefix(dirs_.cwd.size());
            if (filePathSv.starts_with("\\") || filePathSv.starts_with("/"))
            {
                filePathSv.remove_prefix(1);
            }
        }
        report::error("{}: \"{}\"",
            Error("Incorrect results"), filePathSv);
        auto res = test_suite::diffStrings(expectedDocs, generatedDocs);
        report::error("{} lines added", res.added);
        report::error("{} lines removed", res.removed);

        report::error("Diff:\n{}", res.diff);

        if(testArgs.badOption.getValue())
        {
            // Write the .bad.<generator> file
            auto badPath = expectedPath;
            path::replace_extension(badPath, llvm::Twine("bad.").concat(gen_->fileExtension()));
            if (auto exp = writeFile(badPath, generatedDocs); !exp)
            {
                return report::error("{}: \"{}\"", exp.error(), badPath);
            }
            report::info("\"{}\" written", badPath);

            // VFALCO We are calling this over and over again instead of once?
            if(! diffCmdPath_.getError())
            {
                path::replace_extension(badPath, gen_->fileExtension());
                std::array<llvm::StringRef, 5u> args {
                    diffCmdPath_.get(), "-u", "--color", expectedPath, badPath };
                llvm::sys::ExecuteAndWait(diffCmdPath_.get(), args);
            }
        }
    }
    // update action
    else if(testArgs.action == Action::update)
    {
        // Update the expected documentation
        if (auto exp = writeFile(expectedPath, generatedDocs); !exp)
        {
            return report::error("{}: \"{}\"", exp.error(), expectedPath);
        }
        report::info("\"{}\" updated", expectedPath);
        ++results.expectedDocsWritten;
        return;
    }
}

void
TestRunner::
handleDir(
    std::string dirPath,
    Config::Settings const& dirSettings)
{
    report::debug("Visiting directory: \"{}\"", dirPath);

    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    ++results.numberOfDirs;

    // Visit each file in the directory
    std::error_code ec;
    fs::directory_iterator const end{};
    fs::directory_iterator iter(dirPath, ec, false);
    if (ec)
    {
        return report::error("{}: \"{}\"", dirPath, Error(ec));
    }
    while(iter != end)
    {
        if (auto const& entry = *iter;
            entry.type() == fs::file_type::directory_file)
        {
            // Check for a subdirectory-wide config
            Config::Settings subdirSettings = dirSettings;
            std::string const& configPath = files::appendPath(entry.path(), "mrdocs.yml");
            if (files::exists(configPath))
            {
                if (auto exp = Config::Settings::load_file(subdirSettings, configPath, dirs_); !exp)
                {
                    return report::error("Failed to load config file: {}: \"{}\"", exp.error(), configPath);
                }
                if (auto exp = subdirSettings.normalize(dirs_); !exp)
                {
                    return report::error("Failed to normalize config file: {}: \"{}\"", exp.error(), configPath);
                }
            }
            handleDir(entry.path(), subdirSettings);
        }
        else if(
            entry.type() == fs::file_type::regular_file &&
            path::extension(entry.path()).equals_insensitive(".cpp"))
        {
            threadPool_.async(
                [this, dirSettings, filePath = SmallPathString(entry.path())]
                {
                    handleFile(filePath, dirSettings);
                });
        }
        iter.increment(ec);
        if (ec)
        {
            return report::error("{}: \"{}\"", Error(ec), dirPath);
        }
    }
}

void
TestRunner::
checkPath(
    std::string inputPath,
    char const** argv)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // See if inputPath references a file or directory
    inputPath = files::normalizePath(inputPath);
    auto fileType = files::getFileType(inputPath);
    if (!fileType)
    {
        return report::error("{}: \"{}\"", fileType.error(), inputPath);
    }

    // Set the reference directories for the test
    std::string const inputDir = fileType == files::FileType::directory
        ? inputPath
        : files::getParentDir(inputPath);
    dirs_.cwd = inputDir;

    // Check for a directory-wide config
    Config::Settings dirSettings;
    testArgs.apply(dirSettings, dirs_, argv);
    dirSettings.multipage = false;
    dirSettings.sourceRoot = files::appendPath(inputPath, ".");

    std::string const& configPath = files::appendPath(inputDir, "mrdocs.yml");
    if (files::exists(configPath))
    {
        if (auto exp = Config::Settings::load_file(dirSettings, configPath, dirs_); !exp)
        {
            return report::error("Failed to load config file: {}: \"{}\"", exp.error(), configPath);
        }
        if (auto exp = dirSettings.normalize(dirs_); !exp)
        {
            return report::error("Failed to normalize config file: {}: \"{}\"", exp.error(), configPath);
        }
    }

    switch(fileType.value())
    {
    case files::FileType::regular:
    {
        // Require a .cpp file
        if (!path::extension(inputPath).equals_insensitive(".cpp"))
        {
            Error err("not a .cpp file");
            return report::error("{}: \"{}\"",
                err, inputPath);
        }

        handleFile(inputPath, dirSettings);
        threadPool_.wait();
        return;
    }

    case files::FileType::directory:
    {
        // Iterate this directory and all its children
        handleDir(inputPath, dirSettings);
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

} // clang::mrdocs
