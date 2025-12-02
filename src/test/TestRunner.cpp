//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Platform.hpp>
#include "TestArgs.hpp"
#include "TestRunner.hpp"
#include "Support/TextNormalization.hpp"
#include "Support/TestLayout.hpp"
#include "Support/Comparison.hpp"
#include <lib/ConfigImpl.hpp>
#include <lib/CorpusImpl.hpp>
#include <lib/MrDocsCompilationDatabase.hpp>
#include <lib/SingleFileDB.hpp>
#include <lib/Support/ExecuteAndWaitWithLogging.hpp>
#include <lib/Support/Path.hpp>
#include <lib/Support/Report.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Generators.hpp>
#include <mrdocs/Support/Error.hpp>
#include <test_suite/diff.hpp>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <atomic>
#include <iostream>
#include <unordered_set>

namespace mrdocs {

TestRunner::
TestRunner(std::string_view generator)
    : gen_(getGenerators().find(generator))
{
    MRDOCS_ASSERT(gen_ != nullptr);
}

namespace {

/** Build a single-file compilation database with the provided command line. */
SingleFileDB
makeSingleFileDB(llvm::StringRef pathName, std::vector<std::string> cmds)
{
    auto fileName = files::getFileName(pathName);
    auto parentDir = files::getParentDir(pathName);

    cmds.push_back(std::string{ fileName });

    clang::tooling::CompileCommand
        cc(parentDir, fileName, std::move(cmds), parentDir);
    cc.Heuristic = "unit test";
    return SingleFileDB(std::move(cc));
}

/** Ensure the given path refers to a regular .cpp file; report and fail otherwise. */
bool
ensureRegularCpp(llvm::StringRef filePath)
{
    auto ft = files::getFileType(filePath);
    if (!ft)
    {
        report::error("{}: \"{}\"", ft.error(), filePath);
        return false;
    }
    if (ft.value() == files::FileType::not_found) {
        report::error("{}: \"{}\"", Error("file not found"), filePath);
        return false;
    }
    if(ft.value() != files::FileType::regular) {
        report::error("{}: \"{}\"", Error("not a regular file"), filePath);
        return false;
    }
    return true;
}

/** Load and normalize directory-level settings, applying any local mrdocs.yml. */
Expected<Config::Settings>
loadDirSettings(
    std::string const& dirPath,
    Config::Settings dirSettings,
    ReferenceDirectories const& dirs)
{
    dirSettings.sourceRoot = dirPath;
    dirSettings.input = {dirPath};
    std::string const& configPath = files::appendPath(dirPath, "mrdocs.yml");
    bool const hasTagfileOverride = !dirSettings.tagfile.empty();
    if (files::exists(configPath))
    {
        MRDOCS_TRY(Config::Settings::load_file(dirSettings, configPath, dirs));
    }
    MRDOCS_TRY(dirSettings.normalize(dirs));
    // Golden tests shouldn't emit tagfiles unless a test explicitly requests one.
    if (!hasTagfileOverride)
    {
        dirSettings.tagfile.clear();
    }
    return dirSettings;
}

/** Build root settings from CLI args and the effective input directory. */
Expected<Config::Settings>
makeRootSettings(
    std::string const& inputDir,
    char const** argv,
    ReferenceDirectories& dirs)
{
    Config::Settings dirSettings;
    testArgs.apply(dirSettings, dirs, argv);
    dirSettings.multipage = false;
    return loadDirSettings(inputDir, std::move(dirSettings), dirs);
}

/** Bundles the normalized path, detected type, and root settings for an input. */
struct PathContext
{
    files::FileType type;
    std::string inputPath;
    std::string inputDir;
    Config::Settings dirSettings;
};

/** Build PathContext, normalizing the path and loading directory settings. */
Expected<PathContext>
buildPathContext(std::string inputPath, char const** argv, ReferenceDirectories& dirs)
{
    inputPath = files::normalizePath(inputPath);
    auto fileType = files::getFileType(inputPath);
    if (!fileType)
        return Unexpected(fileType.error());

    std::string const inputDir = fileType == files::FileType::directory
        ? inputPath
        : files::getParentDir(inputPath);
    dirs.cwd = inputDir;

    auto dirSettings = makeRootSettings(inputDir, argv, dirs);
    if (!dirSettings)
        return Unexpected(dirSettings.error());

    return PathContext{
        *fileType,
        std::move(inputPath),
        inputDir,
        *dirSettings
    };
}
} // (anon)

void
TestRunner::
handleFile(
    llvm::StringRef filePath,
    Config::Settings const& dirSettings)
{
    report::debug("Handling {}", filePath);

    MRDOCS_ASSERT(llvm::sys::path::extension(filePath).compare_insensitive(".cpp") == 0);
    if (!ensureRegularCpp(filePath))
        return;

    auto resolved = resolveTestLayout(
        filePath, dirSettings, gen_->fileExtension(), dirs_, testArgs.action);
    if (!resolved)
    {
        return report::error("{}: \"{}\"", resolved.error(), filePath);
    }
    Config::Settings fileSettings = std::move(resolved->settings);
    TestLayout layout = std::move(resolved->layout);

    auto expConfig = ConfigImpl::load(fileSettings, dirs_, threadPool_);
    if (!expConfig)
    {
        return report::error("{}: \"{}\"", expConfig.error(), filePath);
    }
    std::shared_ptr<ConfigImpl const> config = *expConfig;

    auto runWith = [&](std::vector<std::string> command)
    {
        auto const db = makeSingleFileDB(filePath, std::move(command));
        MrDocsCompilationDatabase compilations(
            llvm::StringRef(files::getParentDir(filePath)),
            db,
            config,
            std::unordered_map<std::string, std::vector<std::string>>{});
        handleCompilationDatabase(filePath, compilations, config, layout);
    };

    runWith({ "clang", "-std=c++23" });
    runWith({ "clang-cl", "/std:c++23preview" });
}

void
TestRunner::handleCompilationDatabase(
    llvm::StringRef filePath,
    MrDocsCompilationDatabase const& compilations,
    std::shared_ptr<ConfigImpl const> const& config,
    TestLayout const& layout)
{
    report::debug("Building Corpus", filePath);
    auto corpus = CorpusImpl::build(config, compilations);
    if (!corpus)
    {
        return report::error("{}: \"{}\"", corpus.error(), filePath);
    }

    if (layout.mode == OutputMode::SinglePage)
    {
        test_support::SinglePageArgs args{
            layout,
            *gen_,
            **corpus,
            filePath,
            testArgs.action,
            testArgs.badOption.getValue(),
            testArgs.forceOption.getValue(),
            dirs_,
            results
        };
        if (auto exp = test_support::compareSinglePage(args); !exp)
        {
            return report::error("{}: \"{}\"", exp.error(), filePath);
        }
    }
    else
    {
        test_support::MultipageArgs args{
            layout,
            *gen_,
            **corpus,
            testArgs.action,
            testArgs.forceOption.getValue(),
            results
        };
        if (auto exp = test_support::compareMultipage(args); !exp)
        {
            return report::error("{}: \"{}\"", exp.error(), filePath);
        }
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

    std::error_code ec;
    fs::directory_iterator const end{};
    fs::directory_iterator iter(dirPath, ec, false);
    if (ec)
        return report::error("{}: \"{}\"", dirPath, Error(ec));

    while(iter != end)
    {
        auto const& entry = *iter;
        if (entry.type() == fs::file_type::directory_file)
        {
            auto const& subdir = entry.path();
            if (!path::extension(subdir).equals_insensitive(".multipage"))
            {
                auto subdirSettings = loadDirSettings(subdir, dirSettings, dirs_);
                if (!subdirSettings)
                    return report::error("Failed to load config file: {}: \"{}\"", subdirSettings.error(), subdir);
                handleDir(subdir, *subdirSettings);
            }
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
            return report::error("{}: \"{}\"", Error(ec), dirPath);
    }
}

void
TestRunner::
checkPath(
    std::string inputPath,
    char const** argv)
{
    auto ctx = buildPathContext(std::move(inputPath), argv, dirs_);
    if (!ctx)
        return report::error("{}: \"{}\"", ctx.error(), inputPath);

    namespace path = llvm::sys::path;
    switch(ctx->type)
    {
    case files::FileType::regular:
    {
        if (!path::extension(ctx->inputPath).equals_insensitive(".cpp"))
            return report::error("{}: \"{}\"", Error("not a .cpp file"), ctx->inputPath);

        handleFile(ctx->inputPath, ctx->dirSettings);
        threadPool_.wait();
        return;
    }

    case files::FileType::directory:
    {
        handleDir(ctx->inputPath, ctx->dirSettings);
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
