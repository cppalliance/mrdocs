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
#include <mrdocs/Support/Report.hpp>

namespace mrdocs::example {

// tag::load-corpus[]
Expected<Corpus>
loadCorpusFromConfig(
    std::string const& configPath,
    ReferenceDirectories const& dirs,
    char const** argv)
{
    Config config;
    ReferenceDirectories localDirs = dirs;
    localDirs.cwd = std::string(files::getParentDir(configPath));
    // Command-line overrides (if any) are applied on top of the config file and
    // before normalization. Running the example against an installed MrDocs
    // needs none; the in-tree test forwards the built-in directory flags here.
    MRDOCS_TRY(Config::load_file(config, configPath, localDirs, argv));
    // load_file applies the config's log level; this example only wants the
    // breaking-change report, so keep extraction diagnostics quiet.
    report::setMinimumLevel(report::Level::error);
    return Corpus::build(config);
}
// end::load-corpus[]

} // namespace mrdocs::example
