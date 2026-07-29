//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_HBS_ADDONPATHS_HPP
#define MRDOCS_LIB_GEN_HBS_ADDONPATHS_HPP

#include "../../Support/AddonRoots.hpp"
#include "HandlebarsGenerator.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace mrdocs::hbs::addon_paths {

/** Returns the list of addon root directories from the configuration.

    This function collects all valid addon root paths by checking
    the primary addons directory and any supplemental addon directories
    specified in the settings.

    @param settings The configuration settings containing addon paths.
    @return A vector of existing addon root directory paths. The primary
            addons directory (if it exists) appears first, followed by
            any existing supplemental addon directories in their
            configured order.
*/
inline std::vector<std::string>
addonRoots(Config const& settings)
{
    std::vector<std::string> roots;
    roots.reserve(1 + settings.addonsSupplemental.size());

    if (files::exists(settings.addons))
        roots.push_back(settings.addons);

    for (auto const& supplemental : settings.addonsSupplemental)
    {
        if (files::exists(supplemental))
            roots.push_back(supplemental);
    }
    return roots;
}


/** Returns directories containing Handlebars partial templates.

    For each addon root, walks the inheritance chain:
    1. `generator/common/partials/` - shared partials for all formats
    2. for each id in `chain`, in order: `generator/<id>/partials/`

    The chain is given most-general first, most-specific last: for a
    generator `md` that extends `adoc`, the caller passes `{"adoc", "md"}`,
    so adoc's partials override common's, and md's override adoc's.
    For a generator that stands alone, the caller passes a single-element
    chain (just its own id).

    The order preserves root precedence: for each root, common partials
    are loaded first, then the inheritance chain. Later roots (supplemental
    addons) can override partials from earlier roots.

    @param roots The addon root directories to search.
    @param chain The inheritance chain of format ids, most-general first.
    @return A vector of existing partial directories in load order.
*/
inline std::vector<std::string>
partialDirs(
    std::vector<std::string> const& roots,
    std::vector<std::string> const& chain)
{
    std::vector<std::string> dirs;
    dirs.reserve(roots.size() * (1 + chain.size()));

    for (auto const& root : roots)
    {
        auto const commonDir = files::appendPath(root, "generator", "common", "partials");
        if (files::exists(commonDir))
            dirs.push_back(commonDir);

        for (auto const& id : chain)
        {
            auto const formatDir = files::appendPath(root, "generator", id, "partials");
            if (files::exists(formatDir))
                dirs.push_back(formatDir);
        }
    }

    return dirs;
}

/** Overload taking a single format id (no inheritance chain).
*/
inline std::vector<std::string>
partialDirs(std::vector<std::string> const& roots, std::string_view ext)
{
    return partialDirs(roots, std::vector<std::string>{std::string(ext)});
}

/** Returns directories containing JavaScript helper scripts.

    For each addon root, this function looks for helper scripts in:
    1. `generator/common/helpers/` - shared helpers for all formats
    2. `generator/<ext>/helpers/` - format-specific helpers

    The order preserves root precedence: for each root, common helpers
    are loaded first, then format-specific ones. Later roots (supplemental
    addons) can override helpers from earlier roots.

    @param roots The addon root directories to search.
    @param ext The output format extension (e.g., "html", "adoc").
    @return A vector of existing helper directories in load order.
*/
inline std::vector<std::string>
helperDirs(
    std::vector<std::string> const& roots,
    std::vector<std::string> const& chain)
{
    std::vector<std::string> dirs;
    dirs.reserve(roots.size() * (1 + chain.size()));

    for (auto const& root : roots)
    {
        auto const commonDir = files::appendPath(root, "generator", "common", "helpers");
        if (files::exists(commonDir))
            dirs.push_back(commonDir);

        for (auto const& id : chain)
        {
            auto const formatDir = files::appendPath(root, "generator", id, "helpers");
            if (files::exists(formatDir))
                dirs.push_back(formatDir);
        }
    }
    return dirs;
}

/** Overload taking a single format id (no inheritance chain).
*/
inline std::vector<std::string>
helperDirs(std::vector<std::string> const& roots, std::string_view ext)
{
    return helperDirs(roots, std::vector<std::string>{std::string(ext)});
}

/** Returns directories containing layout templates.

    For each addon root, this function looks for layout templates in:
    `generator/<ext>/layouts/`

    Layout templates define the overall page structure (e.g., wrapper.html.hbs,
    index.html.hbs). Later roots can override layouts from earlier roots.

    @param roots The addon root directories to search.
    @param ext The output format extension (e.g., "html", "adoc").
    @return A vector of existing layout directories in load order.
*/
inline std::vector<std::string>
layoutDirs(std::vector<std::string> const& roots, std::string_view ext)
{
    std::vector<std::string> dirs;
    dirs.reserve(roots.size());
    for (auto const& root : roots)
    {
        auto const dir = files::appendPath(root, "generator", ext, "layouts");
        if (files::exists(dir))
            dirs.push_back(dir);
    }
    return dirs;
}

/** Searches addon directories for a specific file.

    Searches through addon roots in reverse order (supplemental addons
    first) to find the specified file. This allows supplemental addons
    to override files from the primary addon directory.

    @param config The configuration containing addon paths.
    @param generator The generator subdirectory (e.g., "html", "common").
    @param subdir The subdirectory within the generator (e.g., "layouts").
    @param filename The filename to search for.
    @return The full path to the file if found, or std::nullopt.
*/
inline std::optional<std::string>
findFile(
    Config const& config,
    std::string_view generator,
    std::string_view subdir,
    std::string_view filename)
{
    auto roots = addon_paths::addonRoots(config);
    for (auto it = roots.rbegin(); it != roots.rend(); ++it)
    {
        std::string candidate = files::appendPath(*it, "generator", generator, subdir, filename);
        if (files::exists(candidate))
            return candidate;
    }
    return std::nullopt;
}

/** Resolves a generator's inheritance chain into format ids.

    Returns the list of format ids that a Builder should consult, in
    load order: the most-general first, the generator's own id last.
    Layouts do not inherit (each format owns its `index.<id>.hbs`),
    so callers that load layouts pass only the leaf id, not the chain.

    Walks the global generator registry via `findGenerator()` to follow
    `HandlebarsGenerator::extends()` references. A reference to a
    generator that is not installed (or is not a HandlebarsGenerator)
    silently terminates the walk; a cycle aborts at the first repeat,
    so a misconfigured manifest cannot loop the lookup.

    @param ext The leaf format id (typically `HandlebarsCorpus::fileExtension`).
    @return The chain, e.g. `{"adoc", "md"}` for a `md` generator that
            extends `adoc`. A standalone generator returns `{ext}`.
*/
inline std::vector<std::string>
extensionChain(std::string_view ext)
{
    std::vector<std::string> chain;
    std::unordered_set<std::string> seen;
    std::string cur(ext);
    while (!cur.empty() && !seen.contains(cur))
    {
        seen.insert(cur);
        chain.push_back(cur);
        Generator const* gen = findGenerator(cur);
        if (!gen)
        {
            break;
        }
        auto const* hbsGen =
            dynamic_cast<HandlebarsGenerator const*>(gen);
        if (!hbsGen)
        {
            break;
        }
        cur = std::string(hbsGen->extends());
    }
    // Caller wants most-general first, most-specific last.
    std::reverse(chain.begin(), chain.end());
    return chain;
}

} // namespace mrdocs::hbs::addon_paths

#endif // MRDOCS_LIB_GEN_HBS_ADDONPATHS_HPP
