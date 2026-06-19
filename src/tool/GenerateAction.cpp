//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ToolArgs.hpp"
#include <lib/CompilationDatabaseBuilder.hpp>
#include <lib/ConfigImpl.hpp>
#include <lib/Support/Chrono.hpp>
#include <lib/CorpusImpl.hpp>
#include <lib/Gen/hbs/DataDrivenGenerators.hpp>
#include <lib/MrDocsCompilationDatabase.hpp>
#include <lib/Support/Path.hpp>
#include <mrdocs/Generator.hpp>
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
    // Discover data-driven generators
    //
    // --------------------------------------------------------------
    // Each <addon>/generator/<name>/ directory that ships its own
    // Handlebars layouts is registered as an additional generator
    // (subject to id and layout-template checks) before the user-
    // requested generator is looked up below.
    MRDOCS_TRY(hbs::discoverDataDrivenGenerators(config->settings()));

    // --------------------------------------------------------------
    //
    // Load generator
    //
    // --------------------------------------------------------------
    auto& settings = config->settings();
    std::vector<Generator const*> generators;
    for (auto const& genId : settings.generator.values)
    {
        MRDOCS_TRY(
            Generator const& generator,
            findGenerator(genId),
            formatError(
                "the Generator \"{}\" was not found",
                genId));
        generators.push_back(&generator);
    }
    MRDOCS_CHECK(!generators.empty(), "No generator was specified");

    // --------------------------------------------------------------
    //
    // Find or generate the compilation database
    //
    // --------------------------------------------------------------
    MRDOCS_TRY(
        MrDocsCompilationDatabase compilationDatabase,
        generateCompilationDatabase(config));

    // --------------------------------------------------------------
    //
    // Build corpus
    //
    // --------------------------------------------------------------
    MRDOCS_TRY(
        std::unique_ptr<Corpus> corpus,
        CorpusImpl::build(config, compilationDatabase));
    // The global namespace is always extracted, so a size of 1 means no
    // declaration other than the global namespace was found. Treat that
    // the same as a truly empty corpus here.
    if (corpus->size() <= 1)
    {
        if (settings.errorOnEmptyCorpus)
        {
            report::error("Corpus is empty, not generating docs");
            return Unexpected(Error("Corpus is empty, not generating docs"));
        }
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
    // Each generator resolves its own output from the configuration and
    // writes whatever files it needs; GenerateAction just runs each one.
    for (Generator const* generator : generators)
    {
        using clock_type = std::chrono::steady_clock;
        auto const start_time = clock_type::now();
        MRDOCS_TRY(generator->build(*corpus));
        report::info(
            "Generated {} documentation in {}",
            generator->displayName(),
            format_duration(clock_type::now() - start_time));
    }

    // --------------------------------------------------------------
    //
    // Clean temp files
    //
    // --------------------------------------------------------------

    return {};
}

} // mrdocs
