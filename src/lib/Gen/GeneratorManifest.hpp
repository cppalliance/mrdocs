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

#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

/** The parsed contents of a generator manifest.

    A manifest is the mrdocs-generator.yml that an addon directory
    under <root>/generator/<name>/ exposes to declare a generator. The
    two generator flavors read disjoint fields of the same file:

    @li A data-driven (Handlebars) generator reads the escape rules
        and the parent it `extends`.

    @li A script-driven generator reads the script-file path and its
        parameters.

    The presence of the `script` entry is what distinguishes the two: a
    manifest that names a `script` is a script-driven generator,
    otherwise it is data-driven.
*/
struct GeneratorManifest
{
    /** The entry file of a script-driven generator.

        Holds the value of the manifest's optional `script` key, a path
        relative to the generator directory. Empty when the manifest
        declares no `script`, in which case the directory is a
        data-driven generator.
    */
    std::optional<std::string> script;

    /** The escape rules of a data-driven generator.

        Each pair maps a byte-sequence source to its replacement string,
        in manifest order. Empty when no escape rules are declared.
    */
    std::vector<std::pair<std::string, std::string>> escape;

    /** The parent a data-driven generator inherits from.

        Holds the manifest's optional `extends` key: the id of another
        generator whose partials and helpers this one falls back to,
        after its own directory but before `common/`. Empty when no
        parent is declared. A script-driven generator ignores this field.
    */
    std::string extends;

    /** The parameters of a script-driven generator.

        Holds the manifest's optional `params` mapping, passed to the
        entry script as its `params` argument. Mapping values may be
        nested objects or arrays; a scalar value is a string. Empty when
        the manifest declares no `params`. A data-driven generator
        ignores this field.
    */
    dom::Object params;
};

/** Parse a generator manifest into plain data.

    Read the file at `yamlPath` and return its contents. The file is
    expected to contain a top-level mapping. The optional `escape` key
    holds a sub-mapping from byte-sequence keys to replacement strings;
    keys may be one or more bytes long, and an empty key is a hard error.
    The optional `script` key holds the entry-file path as a scalar. The
    optional `extends` key names a parent generator as a scalar. The
    optional `params` key holds a mapping of generator-specific
    parameters; its values may be nested, and a scalar value is read as a
    string. Unknown top-level keys are ignored so future schema additions
    are non-breaking.

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

    The presence of a `script` entry distinguishes the two generator
    flavors, so a caller installs the flavor it owns and ignores the
    other. Roots are searched in order, so the result preserves addon
    precedence.
*/
Expected<std::vector<DiscoveredManifest>>
discoverGeneratorManifests(std::vector<std::string> const& roots);

} // mrdocs

#endif
