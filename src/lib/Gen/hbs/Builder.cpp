//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Builder.hpp"
#include "AddonPaths.hpp"
#include <lib/ConfigImpl.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/Report.hpp>
#include <algorithm>
#include <filesystem>
#include <format>
#include <ranges>
#include <vector>


namespace mrdocs {

namespace hbs {

namespace {

/** The active generator's `generator-options.<id>` settings.

    Exposed to templates as `generatorConfig` so shared partials can read a
    per-generator option directly as `@root.generatorConfig.<key>`, avoiding a
    costly generator-options lookup by generator name in the template and
    letting common partials reach generator configuration without hardcoding
    the generator. Returns an empty object when the generator has no entry.
*/
dom::Value
generatorConfig(HandlebarsCorpus const& domCorpus)
{
    auto const& genOpts = domCorpus->config->generatorOptions;
    auto const it = genOpts.find(domCorpus.fileExtension);
    return it != genOpts.end()
        ? dom::Value(it->second)
        : dom::Value(dom::Object());
}

/** Loads Handlebars partial templates from a directory.

    Recursively scans the specified directory for `.hbs` files and
    registers each as a Handlebars partial. The partial name is derived
    from the file's relative path (without extension), allowing
    subdirectory organization (e.g., `components/button.hbs` becomes
    partial `components/button`).

    @param hbs The Handlebars instance to register partials with.
    @param partialsPath The directory path to scan for partial files.
*/
void
loadPartials(
    Handlebars& hbs,
    std::string const& partialsPath)
{
    if (!files::exists(partialsPath))
    {
        return;
    }
    auto exp = forEachFile(partialsPath, true,
        [&](std::string_view pathName) -> Expected<void>
        {
            // Skip directories
            MRDOCS_CHECK_OR(!files::isDirectory(pathName), {});

            // Get template relative path
            std::filesystem::path relPath = pathName;
            relPath = relPath.lexically_relative(partialsPath);

            // Skip non-handlebars files
            MRDOCS_CHECK_OR(relPath.extension() == ".hbs", {});

            // Remove any file extensions
            while(relPath.has_extension())
            {
                relPath.replace_extension();
            }

            // Load partial contents
            MRDOCS_TRY(std::string text, files::getFileText(pathName));

            // Register partial
            hbs.registerPartial(relPath.generic_string(), text);
            return {};
        });
    if (!exp)
    {
        exp.error().Throw();
    }
}

/** Makes a URL relative to another URL.

    This function implements Antora-style URL relativization, creating
    relative URLs between two paths. It takes a target path (`to`) and
    a source path (`from`), returning a relative path from `from` to `to`.

    When called with a single argument, the current symbol's URL (from
    `data.root.symbol.url`) is used as the source path.

    @param to0 The target URL to make relative.
    @param from0 The source URL to relativize from (optional).
    @param options Handlebars options containing context data.
    @return The relative URL path, or the original URL if not absolute.

    @see https://gitlab.com/antora/antora-ui-default/-/blob/master/src/helpers/relativize.js
*/
dom::Value
relativize_fn(dom::Value to0, dom::Value from0, dom::Value options)
{
    if (!to0)
    {
        return "#";
    }

    if (!to0.isString())
    {
        return to0;
    }

    std::string_view to = to0.getString().get();
    if (!to.starts_with('/'))
    {
        return to0;
    }

    // Single argument invocation
    bool const singleArg = !options;
    if (singleArg)
    {
        // NOTE only legacy invocation provides both to and from0
        options = from0;
        from0 = options.lookup("data.root.symbol.url");
    }

    // If `from` is still not set as a string
    if (!from0.isString() || from0.getString().empty())
    {
        return to;
    }
    std::string_view const from = from0.getString().get();

    // Find anchor in URL
    std::string_view hash;
    std::size_t const hashIdx = to.find('#');
    if (hashIdx != std::string_view::npos)
    {
        hash = to.substr(hashIdx);
        to = to.substr(0, hashIdx);
    }

    // Handle the case where they are the same URL
    if (to == from)
    {
        if (!hash.empty())
        {
            return hash;
        }
        if (files::isDirsy(to))
        {
            return "./";
        }
        return files::getFileName(to);
    }

    // Handle the general case
    std::string const fromDir(files::getParentDir(from));
    std::string relativePath = std::filesystem::path(to).lexically_relative(fromDir).generic_string();
    if (relativePath.empty())
    {
        relativePath = ".";
    }
    if (!relativePath.starts_with("../") && !relativePath.starts_with("./"))
    {
        // relative hrefs needs to explicitly include "./" so that
        // they are always treated as relative to the current page
        relativePath = "./" + relativePath;
    }
    relativePath += hash;
    return relativePath;
}

/** Registers partial templates from multiple directories.

    Iterates through each directory and loads all `.hbs` files as
    Handlebars partials. Later directories in the list can override
    partials from earlier ones, enabling supplemental addons to
    customize templates.

    @param hbs The Handlebars instance to register partials with.
    @param dirs The list of directories to load partials from.
*/
void
registerPartials(Handlebars& hbs, std::vector<std::string> const& dirs)
{
    for (auto const& dir : dirs)
        loadPartials(hbs, dir);
}

/** Registers default Handlebars Generator helpers.

    Registers the default set of helpers available in all templates:
    - `primary_location`: Returns the primary source location for a symbol
    - `relativize`: Creates relative URLs between paths
    - Constructor, string, Antora, logical, math, container, and type helpers

    These helpers are registered before user-defined helpers, allowing
    users to override built-in behavior if needed.

    @param hbs The Handlebars instance to register helpers with.
*/
void
registerDefaultHelpers(Handlebars& hbs)
{
    hbs.registerHelper("primary_location",
        dom::makeInvocable([](dom::Value const& v) -> dom::Value
        {
            dom::Value const sourceInfo = v.get("loc");
            if (!sourceInfo)
                return nullptr;

            dom::Value decls = sourceInfo.get("loc");
            if (dom::Value def = sourceInfo.get("defLoc"))
            {
                if (dom::Value const kind = v.get("kind");
                    kind == "record" || kind == "enum")
                    return def;
                if (!decls ||
                    !decls.isArray() ||
                    decls.getArray().empty())
                    return def;
            }
            if (!decls.isArray() || decls.getArray().empty())
                return nullptr;

            for (dom::Value const& loc : decls.getArray())
            {
                if (loc.get("documented"))
                    return loc;
            }
            return decls.getArray().get(0);
        }));

    helpers::registerConstructorHelpers(hbs);
    helpers::registerStringHelpers(hbs);
    helpers::registerAntoraHelpers(hbs);
    helpers::registerLogicalHelpers(hbs);
    helpers::registerMathHelpers(hbs);
    helpers::registerContainerHelpers(hbs);
    helpers::registerTypeHelpers(hbs);
    hbs.registerHelper("relativize", dom::makeInvocable(relativize_fn));
}

/** Categorizes files in the helper directories by extension.

    Walks each directory recursively, picking files whose name ends in
    `ext`. Files whose stem starts with `_` are treated as utility scripts
    (loaded before helpers); the rest are recorded as helpers keyed by
    stem. Other extensions are ignored, so JS and Lua scans do not
    interfere with each other.

    @param helperDirs The directories to scan for helper files.
    @param ext The file extension to match (e.g. `.js`, `.lua`).
    @param[out] utilityFiles Paths of utility scripts to run before helpers.
    @param[out] helperFiles (helper name, path) pairs, in directory order.
    @return Success, or the underlying filesystem error.
*/
Expected<void>
collectHelperFiles(
    std::vector<std::string> const& helperDirs,
    std::string_view ext,
    std::vector<std::string>& utilityFiles,
    std::vector<std::pair<std::string, std::string>>& helperFiles)
{
    for (auto const& dir : helperDirs)
    {
        if (!files::exists(dir))
            continue;

        auto exp = forEachFile(dir, true,
            [&](std::string_view pathName) -> Expected<void>
            {
                if (!pathName.ends_with(ext))
                    return {};
                auto name = files::getFileName(pathName);
                name.remove_suffix(ext.size());

                if (name.starts_with("_"))
                    utilityFiles.emplace_back(pathName);
                else
                    helperFiles.emplace_back(std::string(name), std::string(pathName));
                return {};
            });
        if (!exp)
            return Unexpected(exp.error());
    }
    return {};
}

/** Registers user-defined JavaScript helpers from addon directories.

    Scans the specified directories for JavaScript files and registers
    them as Handlebars helpers. Files are categorized into two types:

    - **Utility files** (prefixed with `_`): Executed as scripts to define
      shared globals. Loaded alphabetically before helper files.
    - **Helper files**: Registered as Handlebars helpers with the filename
      (minus `.js`) as the helper name.

    This separation allows helpers to share common code through utilities
    without duplicating implementations.

    @param hbs The Handlebars instance to register helpers with.
    @param ctx The JavaScript context for script execution.
    @param helperDirs The directories to scan for helper files.
    @return Success, or an error if loading/registration fails.
*/
Expected<void>
registerUserJsHelpers(
    Handlebars& hbs,
    js::Context& ctx,
    std::vector<std::string> const& helperDirs)
{
    // Collect all .js files, separating utilities from helpers.
    // Utility files (starting with '_') define shared globals and are
    // loaded before helper files. This allows helpers to share code
    // without duplicating implementations.
    //
    // Utility files are loaded in alphabetical order to ensure predictable
    // behavior when utilities depend on each other (e.g., _a.js runs before
    // _b.js). If you need complex dependencies, consider consolidating into
    // a single utility file.
    std::vector<std::string> utilityFiles;
    std::vector<std::pair<std::string, std::string>> helperFiles; // (name, path)

    MRDOCS_TRY(collectHelperFiles(helperDirs, ".js", utilityFiles, helperFiles));

    std::sort(utilityFiles.begin(), utilityFiles.end());

    // Load utilities first (they define globals available to helpers).
    // Each utility is loaded in its own scope; globals persist across scopes.
    for (auto const& utilPath : utilityFiles)
    {
        js::Scope scope(ctx);
        MRDOCS_TRY(auto script, files::getFileText(utilPath));
        auto exp = scope.script(script);
        if (!exp)
        {
            return Unexpected(formatError(
                "Error loading utility {}: {}",
                utilPath, exp.error().message()));
        }
    }

    // Load helpers (they can use globals defined by utilities).
    // Each helper is registered in its own scope via js::registerHelper.
    for (auto const& [name, path] : helperFiles)
    {
        MRDOCS_TRY(auto script, files::getFileText(path));
        MRDOCS_TRY(js::registerHelper(hbs, name, ctx, script));
    }

    return {};
}

/** Registers user-defined Lua helpers from addon directories.

    Mirrors @ref registerUserJsHelpers for Lua. Files are categorized:

    - **Utility files** (prefixed with `_`): Loaded as Lua chunks and
      executed once. Use them to populate the global table or `package`
      modules that helpers can reference.
    - **Helper files**: Registered as Handlebars helpers via
      @ref lua::registerHelper, using the filename stem as the helper name.

    Both `.js` and `.lua` files can coexist in the same addon directory.
    A `.lua` helper registered with the same name as an existing `.js`
    helper replaces it (because Handlebars helper registration overwrites).

    @param hbs The Handlebars instance to register helpers with.
    @param ctx The Lua context for script execution.
    @param helperDirs The directories to scan for helper files.
    @return Success, or an error if loading/registration fails.
*/
Expected<void>
registerUserLuaHelpers(
    Handlebars& hbs,
    lua::Context& ctx,
    std::vector<std::string> const& helperDirs)
{
    std::vector<std::string> utilityFiles;
    std::vector<std::pair<std::string, std::string>> helperFiles; // (name, path)

    MRDOCS_TRY(collectHelperFiles(helperDirs, ".lua", utilityFiles, helperFiles));

    std::sort(utilityFiles.begin(), utilityFiles.end());

    for (auto const& utilPath : utilityFiles)
    {
        lua::Scope scope(ctx);
        MRDOCS_TRY(auto script, files::getFileText(utilPath));
        MRDOCS_TRY(auto chunk, scope.loadChunk(script, utilPath));
        auto exp = chunk.call();
        if (!exp)
        {
            return Unexpected(formatError(
                "Error loading utility {}: {}",
                utilPath, exp.error().message()));
        }
    }

    for (auto const& [name, path] : helperFiles)
    {
        MRDOCS_TRY(auto script, files::getFileText(path));
        MRDOCS_TRY(lua::registerHelper(hbs, name, ctx, script));
    }

    return {};
}

/** Loads a layout template from addon directories.

    Searches through the layout directories for the specified template
    file. If found in multiple directories, later directories override
    earlier ones (enabling supplemental addons to customize layouts).

    @param templates The map to store loaded templates (filename -> content).
    @param layoutDirs The directories to search for the template.
    @param filename The template filename to load (e.g., "index.html.hbs").
    @return Success, or throws an error if the template is not found.
 */
Expected<void, Error>
loadLayoutTemplate(
    std::map<std::string, std::string, std::less<>>& templates,
    std::vector<std::string> const& layoutDirs,
    std::string const& filename)
{
    bool loaded = false;
    for (auto const& dir : layoutDirs)
    {
        auto const pathName = files::appendPath(dir, filename);
        if (!files::exists(pathName))
            continue;
        MRDOCS_TRY(auto text, files::getFileText(pathName));
        templates[filename] = std::move(text); // later dirs override
        loaded = true;
    }
    if (!loaded)
    {
        return Unexpected(formatError(
            "Template {} not found in addons search path", filename));
    }
    return {};
}

} // (anon)

Builder::
Builder(
    HandlebarsCorpus const& corpus,
    std::function<void(OutputRef&, std::string_view)> escapeFn)
    : escapeFn_(std::move(escapeFn))
    , domCorpus(corpus)
{
    namespace fs = std::filesystem;

    auto const& config = domCorpus->config;
    auto const roots = addon_paths::addonRoots(config.settings());
    auto const chain = addon_paths::extensionChain(domCorpus.fileExtension);
    auto const partialDirs = addon_paths::partialDirs(roots, chain);
    auto const helperDirs = addon_paths::helperDirs(roots, chain);
    // Layouts do not inherit: each format owns its index.<id>.hbs and
    // wrapper.<id>.hbs because the filename is keyed on the leaf id.
    auto const layoutDirs = addon_paths::layoutDirs(roots, domCorpus.fileExtension);

    // Load partials (later dirs overwrite earlier ones because we walk in order)
    registerPartials(hbs_, partialDirs);

    // Built-in helpers first, then user scripts (JS and Lua) so user code
    // can override built-ins. JS runs before Lua, so a Lua helper with the
    // same name as a JS helper takes precedence (last-write-wins on the
    // Handlebars side).
    registerDefaultHelpers(hbs_);
    if (auto exp = registerUserJsHelpers(hbs_, ctx_, helperDirs); !exp)
        exp.error().Throw();
    if (auto exp = registerUserLuaHelpers(hbs_, lua_ctx_, helperDirs); !exp)
        exp.error().Throw();

    // Load layout templates
    if (auto exp = loadLayoutTemplate(templates_, layoutDirs, std::format("index.{}.hbs", domCorpus.fileExtension)); !exp)
        exp.error().Throw();
    if (auto exp = loadLayoutTemplate(templates_, layoutDirs, std::format("wrapper.{}.hbs", domCorpus.fileExtension)); !exp)
        exp.error().Throw();
}

//------------------------------------------------

Expected<void>
Builder::
callTemplate(
    std::ostream& os,
    std::string_view name,
    dom::Value const& context)
{
    auto it = templates_.find(name);
    MRDOCS_CHECK(it != templates_.end(), formatError("Template {} not found", name));
    std::string_view fileText = it->second;
    HandlebarsOptions options;
    options.escapeFunction = escapeFn_;
    OutputRef out(os);
    Expected<void, HandlebarsError> exp =
        hbs_.try_render_to(out, fileText, context, options);
    if (!exp)
    {
        return Unexpected(Error(exp.error().what()));
    }
    return {};
}

//------------------------------------------------
static std::string
makeRelfileprefix(std::string_view url)
{
    if (!url.starts_with('/'))
        return {};

    std::size_t const depth = static_cast<std::size_t>(
        std::ranges::count(url.substr(1), '/'));

    std::string prefix;
    prefix.reserve(depth * 3);
    for (std::size_t i = 0; i < depth; ++i)
        prefix.append("../");
    return prefix;
}

dom::Object
Builder::
createContext(Symbol const& I)
{
    dom::Object ctx;
    dom::Object page;
    page.set("stylesheets", domCorpus.stylesheets);
    page.set("inlineStyles", domCorpus.inlineStyles);
    page.set("inlineScripts", domCorpus.inlineScripts);
    page.set("hasDefaultStyles", domCorpus.hasDefaultStyles);
    if (domCorpus->config->multipage)
    {
        page.set("relfileprefix", makeRelfileprefix(domCorpus.getURL(I)));
    }
    ctx.set("page", page);
    ctx.set("symbol", domCorpus.get(I.id));
    ctx.set("config", domCorpus->config.object());
    // The active generator's id, so templates can index per-generator
    // settings, e.g. lookup config.generator-options.<generatorId>.
    ctx.set("generatorId", domCorpus.fileExtension);
    ctx.set("generatorConfig", generatorConfig(domCorpus));

    return ctx;
}

template <std::derived_from<Symbol> T>
Expected<void>
Builder::
operator()(std::ostream& os, T const& I)
{
  std::string const templateFile = indexTemplateFile();
  dom::Object const ctx = createContext(I);

    if (auto &config = domCorpus->config;
        config->embedded || !config->multipage) {
    // Single page or embedded pages render the index template directly
    // without the wrapper
    return callTemplate(os, templateFile, ctx);
  }

    // Multipage output: render the wrapper template
    // The context receives the original symbol and the contents from rendering
    // the index template
    auto const wrapperFile = wrapperTemplateFile();
    dom::Object const wrapperCtx = createFrame(ctx);
    wrapperCtx.set("contents", dom::makeInvocable([this, templateFile, ctx, &os](
        dom::Value const&) -> Expected<dom::Value>
        {
            // Helper to write contents directly to stream
            // Reuse the already-built context to avoid recomputing DOM data.
            MRDOCS_TRY(callTemplate(os, templateFile, ctx));
            return {};
        }));
    return callTemplate(os, wrapperFile, wrapperCtx);
}

// Compile the Builder::operator() for each Symbol type
#define INFO(T) template Expected<void> Builder::operator()<T##Symbol>(std::ostream&, T##Symbol const&);
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

Expected<void>
Builder::
renderWrapped(
    std::ostream& os,
    std::function<Expected<void>()> contentsCb)
{
    auto const wrapperFile =
        wrapperTemplateFile();
    dom::Object ctx;
    dom::Object page;
    page.set("stylesheets", domCorpus.stylesheets);
    page.set("inlineStyles", domCorpus.inlineStyles);
    page.set("inlineScripts", domCorpus.inlineScripts);
    page.set("hasDefaultStyles", domCorpus.hasDefaultStyles);
    ctx.set("page", page);
    ctx.set("config", domCorpus->config.object());
    // The active generator's id, so templates can index per-generator
    // settings, e.g. lookup config.generator-options.<generatorId>.
    ctx.set("generatorId", domCorpus.fileExtension);
    ctx.set("generatorConfig", generatorConfig(domCorpus));
    ctx.set("contents",
            dom::makeInvocable([&](dom::Value const &) -> Expected<dom::Value> {
              MRDOCS_TRY(contentsCb());
              return {};
            }));

    return callTemplate(os, wrapperFile, ctx);
}

std::string
Builder::
indexTemplateFile() const
{
    return std::format("index.{}.hbs", domCorpus.fileExtension);
}

std::string
Builder::
wrapperTemplateFile() const
{
    return std::format("wrapper.{}.hbs", domCorpus.fileExtension);
}


} // hbs
} // mrdocs
