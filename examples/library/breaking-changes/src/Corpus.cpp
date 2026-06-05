//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#include "Corpus.hpp"

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/Path.hpp>

namespace mrdocs::example {

// tag::load-corpus[]
Expected<std::unique_ptr<Corpus>>
loadCorpusFromConfig(
    std::string const& configPath,
    ReferenceDirectories const& dirs,
    ThreadPool& threadPool)
{
    Config::Settings settings;
    ReferenceDirectories localDirs = dirs;
    localDirs.cwd = files::getParentDir(configPath);
    MRDOCS_TRY(Config::Settings::load_file(settings, configPath, localDirs));
    MRDOCS_TRY(settings.normalize(localDirs));

    MRDOCS_TRY(auto config, Config::load(settings, localDirs, threadPool));
    return Corpus::build(config);
}
// end::load-corpus[]

} // namespace mrdocs::example
