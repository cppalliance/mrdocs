//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Platform.hpp>
#include <mrdocs/MrDocsCompilationDatabase.hpp>
#include <mrdocs/SingleFileDB.hpp>
#include <mrdocs/Support/Filesystem/Temp.hpp>
#include <mrdocs/Support/ReportImpl.hpp>
#include "Support/Comparison.hpp"
#include "Support/TextNormalization.hpp"
#include "TestCliArgs.hpp"
#include "TestRunner.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Container/Algorithm.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <test_suite/diff.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <algorithm>
#include <iostream>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mrdocs {

namespace {

/** Load and normalize directory-level settings, applying any local mrdocs.yml.
 */
Expected<Config>
loadDirConfig(
    std::string const& dirPath,
    Config dirConfig,
    ReferenceDirectories const& dirs)
{
    // Load YML on top of dirConfig
    dirConfig.sourceRoot = dirPath;
    dirConfig.input = {dirPath};
    std::string const& configPath = files::appendPath(dirPath, "mrdocs.yml");
    bool const hasTagfileOverride = !dirConfig.outputTagfile.empty();
    if (files::exists(configPath))
    {
        MRDOCS_TRY(Config::load_file(dirConfig, configPath));
    }

    // Normalize settings with base paths
    MRDOCS_TRY(dirConfig.normalize(dirs));

    // Golden tests shouldn't emit tagfiles unless a test explicitly requests one.
    if (!hasTagfileOverride)
    {
        dirConfig.outputTagfile.clear();
    }
    return dirConfig;
}

/// Filename marking a directory as a test-suite root (see @ref makeBaseSettings).
constexpr std::string_view testRootMarker = ".mrdocs-test-root";

/** Build the settings and reference directories the input inherits.

    A run can be pointed at a nested directory or a single file, but the
    fixtures assume every ancestor `mrdocs.yml` from the suite root down is
    active. A single walk up from the input does both jobs: it locates the
    suite root (the nearest ancestor holding @ref testRootMarker) and, along
    the way, collects the directories whose `mrdocs.yml` must be replayed,
    from the root down to the input's parent. The input's own `mrdocs.yml`
    is layered afterwards by handleDir/loadDirConfig.

    An explicit marker pins the root unambiguously: unlike "the topmost
    directory that has a `mrdocs.yml`", a stray `mrdocs.yml` above the suite
    cannot silently extend it. Every suite root must carry the marker, so a
    missing one is an error (it catches a run pointed outside a marked
    suite) rather than silently treating the input as the root.

    @param inputDir The input's directory (the input itself if a directory,
        or its parent if a single file).
    @param argv The command line, applied as the lowest-priority layer.
*/
Expected<std::pair<Config, ReferenceDirectories>>
makeBaseSettings(
    std::string_view inputDir,
    char const** argv)
{
    // Walk from the input dir to the marked suite root.
    std::string_view root = inputDir;
    while (!files::exists(files::appendPath(root, testRootMarker)))
    {
        std::string_view const parent = files::getParentDir(root);
        MRDOCS_CHECK_OR(parent != root, Unexpected(formatError(
            "no test-suite root marker (\"{}\") found at or above \"{}\"",
            testRootMarker, inputDir)));
        root = parent;
    }

    // Resolve reference directories relative to the root
    ReferenceDirectories dirs;
    dirs.cwd = std::string(root);
    dirs.mrdocsRoot = files::getParentDir(root, 2);

    // Load base configuration files from root to inputDir. The command line is
    // the lowest-priority layer for tests: load it alone first (empty YAML, argv only),
    // then merge the config files on top.
    Config settings;
    MRDOCS_TRY(Config::load(settings, "", argv));
    for (std::string_view dir = root; dir != inputDir; )
    {
        MRDOCS_TRY(settings, loadDirConfig(std::string(dir), std::move(settings), dirs));
        std::size_t const slash = inputDir.find('/', dir.size() + 1);
        dir = inputDir.substr(
            0, slash == std::string_view::npos ? inputDir.size() : slash);
    }
    return std::pair{std::move(settings), std::move(dirs)};
}

/** The per-file settings, loaded before the generator (and its extension)
    is known so addon discovery can run against them in between.
*/
struct LoadedTestSettings
{
    /// Directory settings with any per-file mrdocs.yml merged in.
    Config settings;
    /// Whether a per-file mrdocs.yml was found and merged.
    bool hasFileConfig = false;
    /// The directory-level multipage flag, snapshotted before the merge
    /// (multipage may only be enabled per file).
    bool dirMultipage = false;
};

/** Merge any per-file mrdocs.yml on top of the directory settings.

    Kept separate from buildTestLayout so addon discovery can run against the
    merged settings in between, before the generator is known.
*/
Expected<LoadedTestSettings>
loadTestSettings(
    llvm::StringRef filePath,
    Config const& dirConfig,
    ReferenceDirectories const& dirs)
{
    LoadedTestSettings result;
    result.settings = dirConfig;
    result.dirMultipage = dirConfig.multipage;
    std::string const configPath = files::withExtension(filePath, "yml");
    result.hasFileConfig = files::exists(configPath);
    if (result.hasFileConfig)
    {
        MRDOCS_TRY(Config::load_file(result.settings, configPath));
    }
    return result;
}

/** Finalize a test file's settings and create its scratch output directory.

    Points `output` at a fresh temporary directory the generator writes
    straight into, normalizes, and validates that the on-disk fixtures match
    the configured mode. Returns the finalized settings paired with the
    scratch directory (empty for the no-op generator, which has no output).
*/
Expected<std::pair<Config, std::optional<ScopedTempDirectory>>>
buildTestLayout(
    llvm::StringRef filePath,
    LoadedTestSettings loaded,
    llvm::StringRef generatorExtension,
    ReferenceDirectories const& dirs,
    Action action)
{
    bool const dirMultipage = loaded.dirMultipage;
    bool const hasFileConfig = loaded.hasFileConfig;
    Config settings = std::move(loaded.settings);
    bool const hasTagfileOverride = !settings.outputTagfile.empty();

    // The no-op generator produces no output: normalize and return without a
    // scratch directory.
    if (generatorExtension.empty())
    {
        MRDOCS_TRY(settings.normalize(dirs));
        if (!hasTagfileOverride)
            settings.outputTagfile.clear();
        return std::pair{std::move(settings), std::optional<ScopedTempDirectory>{}};
    }

    // The generator writes straight into a fresh temporary directory (one per
    // generator, so there is no per-format subdirectory.
    std::optional<ScopedTempDirectory> scratch(std::in_place, "mrdocs-test-output");
    MRDOCS_CHECK_OR(!scratch->failed(), Unexpected(scratch->error()));
    settings.output = std::string(scratch->path());
    if (!settings.outputTagfile.empty() &&
        !llvm::sys::path::is_absolute(settings.outputTagfile))
        settings.outputTagfile = files::appendPath(scratch->path(), settings.outputTagfile);
    MRDOCS_TRY(settings.normalize(dirs));
    if (!hasTagfileOverride)
        settings.outputTagfile.clear();

    // Validate that the on-disk fixtures match the configured mode. The
    // comparison recomputes these paths the same way.
    std::string const multipageRoot = files::withExtension(filePath, "multipage");
    MRDOCS_TRY(auto const single,
        files::getFileType(files::withExtension(filePath, generatorExtension)));
    MRDOCS_TRY(auto const multi, files::getFileType(multipageRoot));
    MRDOCS_TRY(auto const multiFmt,
        files::getFileType(files::appendPath(multipageRoot, generatorExtension)));
    bool const singleExists = single == files::FileType::regular;
    bool const multiExists = multi == files::FileType::directory;
    bool const multiFmtExists = multiFmt == files::FileType::directory;

    if (settings.multipage)
    {
        MRDOCS_CHECK(hasFileConfig,
            Error("multipage tests require a per-file mrdocs.yml with multipage: true"));
        MRDOCS_CHECK(!dirMultipage,
            Error("multipage defaults must remain disabled at the directory level"));
        MRDOCS_CHECK(!singleExists,
            Error("multipage test cannot have single-page expected outputs"));
        MRDOCS_CHECK(single != files::FileType::directory,
            Error("unexpected directory where single-page expectation would be"));
        MRDOCS_CHECK(multi != files::FileType::regular,
            Error("multipage snapshot path must be a directory"));
        MRDOCS_CHECK(action != Action::test || multiFmtExists,
            Error("missing multipage snapshot for generator"));
    }
    else
    {
        MRDOCS_CHECK(!multiExists && !multiFmtExists,
            Error("single-page test cannot have a .multipage snapshot"));
        MRDOCS_CHECK(action != Action::test || singleExists,
            Error("missing test file"));
    }

    return std::pair{std::move(settings), std::move(scratch)};
}

} // (anon)

Expected<void>
TestRunner::
checkPath(
    std::string inputPath,
    char const** argv)
{
    namespace path = llvm::sys::path;

    // Validate input
    inputPath = files::normalizePath(inputPath);
    MRDOCS_TRY(auto const fileType, files::getFileType(inputPath));
    MRDOCS_CHECK(
        is_one_of(fileType, {files::FileType::directory, files::FileType::regular}),
        Error("input path must be a directory or a regular file"));
    bool isDirectory = fileType == files::FileType::directory;
    MRDOCS_CHECK(
        isDirectory ||
        path::extension(inputPath).equals_insensitive(".cpp"),
        Error("input file must be a .cpp file"));


    // Load the inherited settings and the reference directories they
    // resolved. The runner keeps neither: both are threaded down from here.
    std::string const inputDir =
        isDirectory ? inputPath : std::string(files::getParentDir(inputPath));
    MRDOCS_TRY_BIND((base, dirs), makeBaseSettings(inputDir, argv));

    if (isDirectory)
    {
        MRDOCS_TRY(handleDir(inputPath, base, dirs));
    }
    else
    {
        // Load the file's directory settings and call handleFile directly
        MRDOCS_TRY(
            Config dirConfig,
            loadDirConfig(inputDir, std::move(base), dirs));
        MRDOCS_TRY(handleFile(inputPath, dirConfig, dirs));
    }

    threadPool_.wait();
    return {};
}

Expected<void>
TestRunner::
handleDir(
    std::string dirPath,
    Config const& parentSettings,
    ReferenceDirectories const& dirs)
{
    report::debug("Visiting directory: \"{}\"", dirPath);

    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    // Layer this directory's mrdocs.yml on top of the inherited settings.
    // Every directory is loaded the same way, so the recursion is uniform
    // and the starting directory needs no special handling.
    MRDOCS_TRY(
        Config dirConfig,
        loadDirConfig(dirPath, parentSettings, dirs));

    ++results.numberOfDirs;

    std::error_code ec;
    fs::directory_iterator const end{};
    fs::directory_iterator iter(dirPath, ec, false);
    MRDOCS_CHECK(!ec, Error(ec));

    while(iter != end)
    {
        auto const& entry = *iter;
        if (entry.type() == fs::file_type::directory_file)
        {
            auto const& subdir = entry.path();
            if (!path::extension(subdir).equals_insensitive(".multipage"))
            {
                MRDOCS_TRY(handleDir(subdir, dirConfig, dirs));
            }
        }
        else if(
            entry.type() == fs::file_type::regular_file &&
            path::extension(entry.path()).equals_insensitive(".cpp"))
        {
            // Files run concurrently, so an error can't propagate out of the
            // task. Report it here; that still fails the run (errorCount > 0)
            // and lets every other fixture finish and report independently.
            // dirs is copied into the task since it outlives this frame.
            threadPool_.async(
                [this, settings = dirConfig, dirs,
                 filePath = SmallPathString(entry.path())]
                {
                if (auto exp = handleFile(filePath, settings, dirs); !exp)
                {
                    report::error("{}: \"{}\"", exp.error(), filePath);
                }
            });
        }
        iter.increment(ec);
        MRDOCS_CHECK(!ec, Error(ec));
    }
    return {};
}

Expected<void>
TestRunner::
handleFile(
    llvm::StringRef filePath,
    Config const& dirConfig,
    ReferenceDirectories const& dirs)
{
    report::debug("Handling {}", filePath);

    MRDOCS_ASSERT(
        llvm::sys::path::extension(filePath).compare_insensitive(".cpp") == 0);
    MRDOCS_CHECK(
        files::isRegularFile(filePath),
        Error("not a regular file"));

    // Load the per-file mrdocs.yml first so data-driven generators
    // contributed via addons-supplemental are visible to discovery
    // before the chosen generator is looked up.
    //
    // The generator registry is process-global and persists across
    // tests. `discoverDataDrivenGenerators` is idempotent (it skips ids
    // already installed), so re-running it per fixture is safe; but
    // it also means the first fixture that registers a given id
    // wins, and a later fixture that ships a generator directory
    // with the same id will see its own contents quietly ignored.
    MRDOCS_TRY(
        LoadedTestSettings loaded,
        loadTestSettings(filePath, dirConfig, dirs));
    MRDOCS_TRY(hbs::discoverDataDrivenGenerators(loaded.settings));

    // The generator(s) come from the test's merged configuration (the
    // directory chain supplies a default), in single, list, or
    // comma-separated form.
    StringList genList = loaded.settings.generator;
    genList.splitCommaSeparated();

    for (std::string const& genId : genList.values)
    {
        Generator const* gen = findGenerator(genId);
        MRDOCS_CHECK(
            gen, formatError("the Generator \"{}\" was not found", genId));
        // scratch owns the temp output directory across this generator's
        // build+compare; the binding keeps it alive for that scope.
        MRDOCS_TRY_BIND(
            (fileSettings, scratch),
            buildTestLayout(
                filePath, loaded, gen->fileExtension(), dirs, testCliArgs.action));

        // Run this iteration as a single-generator configuration so that
        // it doesn't write into a per-id subdirectory.
        fileSettings.generator = StringList({genId});
        Config const& config = fileSettings;

        auto runWith = [&](std::vector<std::string> command) -> Expected<void>
        {
            auto const db = SingleFileDB::create(filePath, std::move(command));
            // A dedicated include directory outside every fixture's
            // source-root. Fixtures reach out-of-tree headers here as
            // <...> system includes (see config/base-url), and the
            // missing-include shim/prefix mechanism resolves its virtual
            // headers against it too.
            std::unordered_map<std::string, std::vector<std::string>>
                defaultIncludePaths = {
                    {    "clang", { MRDOCS_TEST_FILES_DIR "/external-include" } },
                    { "clang-cl", { MRDOCS_TEST_FILES_DIR "/external-include" } },
            };
            MrDocsCompilationDatabase compilations(
                llvm::StringRef(files::getParentDir(filePath)),
                db,
                config,
                defaultIncludePaths);
            return handleCompilationDatabase(
                filePath, *gen, compilations, config);
        };

        MRDOCS_TRY(runWith({ "clang", "-std=c++23" }));
        MRDOCS_TRY(runWith({ "clang-cl", "/std:c++23preview" }));
    }
    return {};
}

Expected<void>
TestRunner::handleCompilationDatabase(
    llvm::StringRef filePath,
    Generator const& gen,
    MrDocsCompilationDatabase const& compilations,
    Config const& config)
{
    report::debug("Building Corpus", filePath);
    MRDOCS_TRY(auto corpus, Corpus::build(config, compilations));

    // Extensions live outside the corpus: load the config's scripts and
    // apply their transforms after the build, before generating. Empty and
    // a no-op when the fixture declares no extensions.
    MRDOCS_TRY(ExtensionRegistry extensions, ExtensionRegistry::load(config));
    MRDOCS_TRY(extensions.applyTransforms(corpus, config));

    if (gen.fileExtension().empty())
    {
        // No-op generator: extraction succeeded and any diagnostics have
        // already been reported. There is no expected output to compare.
        report::info("\"{}\" extracted", filePath);
        ++results.expectedDocsMatching;
        return {};
    }

    MRDOCS_TRY(test_support::generateAndCompareOutput({
        .gen = gen,
        .corpus = corpus,
        .config = config,
        .filePath = filePath,
        .action = testCliArgs.action,
        .writeBad = testCliArgs.bad,
        .forceUpdate = testCliArgs.force,
        .results = results
    }));
    return {};
}

} // mrdocs
