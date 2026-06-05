//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//

#ifndef MRDOCS_EXAMPLE_BREAKING_CHANGES_CORPUS_HPP
#define MRDOCS_EXAMPLE_BREAKING_CHANGES_CORPUS_HPP

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/ThreadPool.hpp>
#include <memory>
#include <string>

namespace mrdocs::example {

// Load a corpus from an `mrdocs.yml` configuration file.
//
// Reads the YAML into a Config::Settings, normalizes it against
// `dirs`, hands it to Config::load, and calls Corpus::build with
// the resulting Config. The compilation database is resolved from
// the configuration (a compile_commands.json, a CMakeLists.txt,
// or synthesized from source-root + input).
Expected<std::unique_ptr<Corpus>>
loadCorpusFromConfig(
    std::string const& configPath,
    ReferenceDirectories const& dirs,
    ThreadPool& threadPool);

} // namespace mrdocs::example

#endif
