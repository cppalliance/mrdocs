//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// A program that loads plugins into one process three times: the `mrdocs`
// tool loads them once per run, so it cannot show what a second load does.
// It is linked the way the tool is, so that a plugin finds MrDocs in it.
//
// The plugin installs a generator, so its entry point failing to be skipped
// the second time shows as a failed load. Its transform appends an X to the
// name of the symbol Widget, so the name counts the runs of the transform.
// The same transform, shared by two registries, runs once for each.
//
// One library registry serves the sessions, as it does a program that runs
// more than one configuration. The two sessions hold the same generator
// object, which is how the plugin shows as loaded once.
//
// Usage: reload-host <addon-root>. The exit status is 0 when all is well.

#include <mrdocs/Config.hpp>
#include <mrdocs/Config/ReferenceDirectories.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Plugin.hpp>
#include <mrdocs/Session.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Reporter.hpp>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>

using namespace mrdocs;

namespace {

void
writeFile(std::string_view path, std::string_view content)
{
    std::ofstream os(std::string{path}, std::ios::binary | std::ios::trunc);
    os.write(content.data(), static_cast<std::streamsize>(content.size()));
}

// A scratch directory that is removed when the program ends.
struct ScratchDirectory
{
    std::filesystem::path path;

    ScratchDirectory()
    {
        std::random_device random;
        std::error_code ec;
        for (int attempt = 0; attempt < 16; ++attempt)
        {
            std::filesystem::path const candidate =
                std::filesystem::temp_directory_path(ec) /
                ("mrdocs-reload-src-" + std::to_string(random()));
            if (ec)
            {
                return;
            }
            if (std::filesystem::create_directory(candidate, ec) && !ec)
            {
                path = candidate;
                return;
            }
        }
    }

    ~ScratchDirectory()
    {
        if (!path.empty())
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    }

    ScratchDirectory(ScratchDirectory const&) = delete;
    ScratchDirectory& operator=(ScratchDirectory const&) = delete;
};

bool
check(bool ok, std::string_view what)
{
    if (!ok)
    {
        std::cerr << "FAILED: " << what << '\n';
    }
    return ok;
}

bool
check(Expected<void> const& result, std::string_view what)
{
    if (!result)
    {
        std::cerr << "FAILED: " << what << ": "
                  << result.error().reason() << '\n';
    }
    return result.has_value();
}

bool
check(Expected<Session> const& result, std::string_view what)
{
    if (!result)
    {
        std::cerr << "FAILED: " << what << ": "
                  << result.error().reason() << '\n';
    }
    return result.has_value();
}

// The name of the symbol that was declared as Widget.
std::string
widgetName(Corpus const& corpus)
{
    std::string name = "none";
    for (Symbol const& symbol : corpus)
    {
        if (symbol.Name.starts_with("Widget"))
        {
            name = symbol.Name;
        }
    }
    return name;
}

// Loads the extensions of a configuration, runs their transforms and finds the xml generator.
Expected<void>
loadAndRun(Config const& config, Corpus& corpus)
{
    MRDOCS_TRY(Session session, Session::create(config));
    MRDOCS_TRY(session.applyTransforms(corpus));
    Generator const* generator = session.generators().find("xml");
    if (!generator)
    {
        return Unexpected(Error("the xml generator is not in the session"));
    }
    return {};
}

} // (anon)

int
main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: reload-host <addon-root>\n";
        return 2;
    }
    ScratchDirectory const src;
    if (src.path.empty())
    {
        std::cerr << "FAILED: no scratch directory\n";
        return 2;
    }
    std::u8string const srcText = src.path.u8string();
    std::string const srcDir(srcText.begin(), srcText.end());
    writeFile(files::appendPath(srcDir, "input.cpp"), "struct Widget {};\n");
    std::string const configPath = files::appendPath(srcDir, "mrdocs.yml");
    writeFile(configPath,
        "source-root: " + srcDir + "\n"
                                   "addons: " +
            srcDir + "\n"
                     "addons-supplemental:\n  - " +
            std::string(argv[1]) + "\n"
                                   "input:\n  - " +
            srcDir + "\n");
    ReferenceDirectories dirs;
    dirs.cwd = srcDir;
    dirs.mrdocsRoot = srcDir;
    // The sessions below share the corpus, and it outlives each of them, so
    // the host builds it with a reporter of its own, declared above the
    // corpus.
    report::Reporter reporter;
    Config config;
    if (!check(Config::loadFile(config, configPath, dirs, reporter), "config"))
    {
        return 1;
    }
    Expected<Corpus> built = Corpus::build(config, reporter);
    if (!built)
    {
        std::cerr << "FAILED: corpus: " << built.error().reason() << '\n';
        return 1;
    }
    Corpus corpus = std::move(*built);

    bool ok = true;
    {
        auto libraries = std::make_shared<ExtensionRegistry::LibraryRegistry>();
        Expected<Session> first = Session::create(config, libraries);
        ok &= check(first, "first load");
        Expected<Session> second = Session::create(config, libraries);
        ok &= check(second, "load into a second session");
        Expected<Session> again = Session::create(config, libraries);
        ok &= check(again, "load into a third session over the same libraries");
        if (!first || !second || !again)
        {
            return 1;
        }
        Generator const* const installed =
            first->generators().find("reload-test");
        ok &= check(installed != nullptr,
            "the generator of the plugin is in the first session");
        ok &= check(installed == second->generators().find("reload-test"),
            "the second session holds the generator the first load made");
        ok &= check(installed == again->generators().find("reload-test"),
            "a session loaded later holds it too, so the entry point ran once");

        ok &= check(widgetName(corpus) == "Widget", "name before");
        ok &= check(first->applyTransforms(corpus), "first apply");
        ok &= check(widgetName(corpus) == "WidgetX",
            "the first session ran the transform once");
        ok &= check(second->applyTransforms(corpus), "second apply");
        ok &= check(widgetName(corpus) == "WidgetXX",
            "the second session got the transform too");

        // A script that registers a generator of its own id loads next to the
        // plugin's, and a session loaded again over the same libraries does
        // not run the entry point of the plugin a second time.
        std::string const extensions = files::appendPath(srcDir, "extensions");
        ok &= check(files::createDirectory(extensions),
            "extensions directory");
        writeFile(files::appendPath(extensions, "z.lua"),
            "mrdocs.register_generator("
            "\"reload-test-script\", function(ctx) end)\n");
        Expected<Session> third = Session::create(config, libraries);
        ok &= check(third,
            "load with a script that registers a generator");
        Expected<Session> thirdAgain = Session::create(config, libraries);
        ok &= check(thirdAgain, "load into a session over the same libraries again");
        if (!third || !thirdAgain)
        {
            return 1;
        }
        ok &= check(
            third->generators().find("reload-test") == installed,
            "the plugin's generator is still the one installed");
        ok &= check(
            third->generators().find("reload-test-script") != nullptr,
            "the script's generator is there");
        ok &= check(
            thirdAgain->generators().find("reload-test") == installed,
            "the plugin's generator is not installed twice");
    }
    ok &= check(loadAndRun(config, corpus), "load and run");
    return ok ? 0 : 1;
}
