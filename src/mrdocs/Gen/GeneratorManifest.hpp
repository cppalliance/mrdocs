//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_GENERATORMANIFEST_HPP
#define MRDOCS_LIB_GEN_GENERATORMANIFEST_HPP

#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

/** The parsed contents of a generator manifest.

    A manifest is the mrdocs-generator.yml that an addon directory under
    <root>/generator/<name>/ exposes to declare a data-driven Handlebars
    generator: it carries the escape rules the generator applies to
    rendered output and the parent it `extends`.
*/
struct GeneratorManifest
{
    /** The escape rules of the generator.

        Each pair maps a byte-sequence source to its replacement string,
        in manifest order. Empty when no escape rules are declared.
    */
    std::vector<std::pair<std::string, std::string>> escape;

    /** The parent this generator inherits from.

        Holds the manifest's optional `extends` key: the id of another
        generator whose partials and helpers this one falls back to,
        after its own directory but before `common/`. Empty when no
        parent is declared.
    */
    std::string extends;
};

/** Parse a generator manifest into plain data.

    Read the file at `yamlPath` and return its contents. The file is
    expected to contain a top-level mapping. The optional `escape` key
    holds a sub-mapping from byte-sequence keys to replacement strings;
    keys may be one or more bytes long, and an empty key is a hard error.
    The optional `extends` key names a parent generator as a scalar.
    Unknown top-level keys are ignored so future schema additions are
    non-breaking.

    An empty document (an empty file, comments only, or a literal `null`)
    yields an empty manifest.
*/
Expected<GeneratorManifest>
loadGeneratorManifest(std::string_view yamlPath);

/** A generator directory paired with its parsed manifest.
*/
struct DiscoveredManifest
{
    /** The generator directory, of the form <root>/generator/<name>.
    */
    std::string dir;

    /** The parsed contents of the directory's manifest.
    */
    GeneratorManifest manifest;
};

/** Find every addon generator directory that ships a manifest.

    For each addon root, walk the immediate subdirectories of
    <root>/generator/. A subdirectory is reported when it ships a
    mrdocs-generator.yml; the manifest is parsed and returned alongside
    its directory. Directories without a manifest (like the built-in
    common/) are skipped.

    Roots are searched in order, so the result preserves addon
    precedence.
*/
Expected<std::vector<DiscoveredManifest>>
discoverGeneratorManifests(std::vector<std::string> const& roots);

} // mrdocs

#endif
