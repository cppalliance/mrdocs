//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//

#ifndef MRDOCS_EXAMPLE_BREAKING_CHANGES_CORPUS_HPP
#define MRDOCS_EXAMPLE_BREAKING_CHANGES_CORPUS_HPP

#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <memory>
#include <string>

namespace mrdocs::example {

// Load a corpus from an `mrdocs.yml` configuration file.
//
// Reads the YAML into a Config, normalizes it against `dirs`, and calls
// Corpus::build with it. The compilation database is resolved from the
// configuration (a compile_commands.json, a CMakeLists.txt, or synthesized
// from source-root + input).
Expected<Corpus>
loadCorpusFromConfig(
    std::string const& configPath,
    ReferenceDirectories const& dirs,
    char const** argv = nullptr);

} // namespace mrdocs::example

#endif
