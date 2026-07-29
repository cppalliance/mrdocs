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

namespace mrdocs::example {

// tag::load-corpus[]
Expected<Corpus>
loadCorpusFromConfig(
    std::string const& configPath,
    ReferenceDirectories const& dirs)
{
    Config config;
    ReferenceDirectories localDirs = dirs;
    localDirs.cwd = std::string(files::getParentDir(configPath));
    MRDOCS_TRY(Config::load_file(config, configPath));
    MRDOCS_TRY(config.normalize(localDirs));
    return Corpus::build(config);
}
// end::load-corpus[]

} // namespace mrdocs::example
