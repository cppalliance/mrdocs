//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_HBS_DATADRIVENGENERATORS_HPP
#define MRDOCS_LIB_GEN_HBS_DATADRIVENGENERATORS_HPP

#include <lib/Gen/hbs/HandlebarsGenerator.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Support/Error.hpp>
#include <string_view>

namespace mrdocs::hbs {

/** Discover data-driven Handlebars generators and install them.

    For each configured addon root, walk the immediate subdirectories of
    <root>/generator/. A subdirectory <name> is treated as a
    data-driven generator when:

    1. No generator with id `<name>` is already registered (so the
       built-in `html` and `adoc` generators take precedence over their
       addon directories of the same name).

    2. It ships an `mrdocs-generator.yml` file. The file's presence is
       the explicit opt-in; directories that hold only shared assets
       (the built-in `common/` is the canonical example) don't declare
       a manifest and are skipped.

    For each accepted directory, a `HandlebarsGenerator` is constructed
    with id, file extension, and display name all set to `<name>`, and
    installed into the global registry. Escape rules are read from
    <name>/mrdocs-generator.yml (see the file format documentation).

    Should be called once after the configuration is resolved and before
    a generator is looked up by id.
*/
Expected<void>
discoverDataDrivenGenerators(Config::Settings const& settings);

/** Load mrdocs-generator.yml and return the resulting `EscapeMap`.

    A thin convenience over `loadGeneratorManifest` (see
    <lib/Gen/GeneratorManifest.hpp>) that keeps only the escape rules,
    for callers that render output and don't need the other manifest
    fields. Parsing rules and errors are as documented there.
*/
Expected<EscapeMap>
loadGeneratorMetadata(std::string_view yamlPath);

} // namespace mrdocs::hbs

#endif
