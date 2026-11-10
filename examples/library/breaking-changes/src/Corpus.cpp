//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//

#include "Corpus.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/Reporter.hpp>

namespace mrdocs::example {

// tag::load-corpus[]
Expected<Corpus>
loadCorpusFromConfig(
    std::string const& configPath,
    ReferenceDirectories const& dirs,
    char const** argv,
    report::Reporter& reporter)
{
    Config config;
    Expected<std::string> absolute = files::makeAbsolute(configPath);
    if (!absolute)
    {
        return Unexpected(absolute.error());
    }
    std::string const& absolutePath = *absolute;
    ReferenceDirectories localDirs = dirs;
    localDirs.cwd = std::string(files::getParentDir(absolutePath));
    // Command-line overrides (if any) are applied on top of the config file and
    // before normalization. Running the example against an installed MrDocs
    // needs none; the in-tree test forwards the built-in directory flags here.
    Expected<void> loaded =
        Config::loadFile(config, absolutePath, localDirs, argv, reporter);
    if (!loaded)
    {
        return Unexpected(loaded.error());
    }
    // loadFile sets the reporter to the config's log level; this example
    // only wants the breaking-change report, so keep extraction diagnostics
    // quiet.
    reporter.setMinimumLevel(report::Level::error);
    return Corpus::build(config, reporter);
}
// end::load-corpus[]

} // namespace mrdocs::example
