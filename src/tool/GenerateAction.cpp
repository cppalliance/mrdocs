//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ToolArgs.hpp"
#include "ToolCompilationDatabase.hpp"
#include <lib/ConfigImpl.hpp>
#include <lib/CorpusImpl.hpp>
#include <lib/MrDocsCompilationDatabase.hpp>
#include <lib/Support/Path.hpp>
#include <mrdocs/Generators.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Support/ThreadPool.hpp>

namespace mrdocs {


Expected<void>
DoGenerateAction(
    std::string const& configPath,
    ReferenceDirectories const& dirs,
    char const** argv)
{
    // --------------------------------------------------------------
    //
    // Load configuration
    //
    // --------------------------------------------------------------
    Config::Settings publicSettings;
    MRDOCS_TRY(Config::Settings::load_file(publicSettings, configPath, dirs));
    MRDOCS_TRY(toolArgs.apply(publicSettings, dirs, argv));
    MRDOCS_TRY(publicSettings.normalize(dirs));
    report::setMinimumLevel(static_cast<report::Level>(publicSettings.logLevel));
    ThreadPool threadPool(publicSettings.concurrency);
    MRDOCS_TRY(
        std::shared_ptr<ConfigImpl const> config,
        ConfigImpl::load(publicSettings, dirs, threadPool));

    // --------------------------------------------------------------
    //
    // Load generator
    //
    // --------------------------------------------------------------
    auto& settings = config->settings();
    MRDOCS_TRY(
        Generator const& generator,
        getGenerators().find(to_string(settings.generator)),
        formatError(
            "the Generator \"{}\" was not found",
            to_string(config->settings().generator)));

    // --------------------------------------------------------------
    //
    // Find or generate the compilation database
    //
    // --------------------------------------------------------------
    ScopedTempDirectory tempDir(
        config->settings().outputDir(),
        ".temp");
    if (tempDir.failed())
    {
        report::error("Failed to create temporary directory: {}", tempDir.error());
        return Unexpected(tempDir.error());
    }
    MRDOCS_TRY(
        MrDocsCompilationDatabase compilationDatabase,
        generateCompilationDatabase(tempDir.path(), config));

    // --------------------------------------------------------------
    //
    // Build corpus
    //
    // --------------------------------------------------------------
    MRDOCS_TRY(
        std::unique_ptr<Corpus> corpus,
        CorpusImpl::build(config, compilationDatabase));
    if (corpus->empty())
    {
        report::warn("Corpus is empty, not generating docs");
        return {};
    }

    // --------------------------------------------------------------
    //
    // Generate docs
    //
    // --------------------------------------------------------------
    // Normalize outputPath path
    MRDOCS_CHECK(settings.output, "The output path argument is missing");
    report::info("Generating docs");
    MRDOCS_TRY(generator.build(*corpus));

    // --------------------------------------------------------------
    //
    // Clean temp files
    //
    // --------------------------------------------------------------

    return {};
}

} // mrdocs
