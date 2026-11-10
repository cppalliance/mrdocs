//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "../Support/DescribedToDom/CachedDescribedObject.hpp"
#include "../Support/PluginLoader.hpp"
#include "../Support/Report/ReportDom.hpp"
#include "AddonDiscovery.hpp"
#include "JsBinding.hpp"
#include "LoadedExtensions.hpp"
#include "LuaBinding.hpp"
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom/OverlayObject.hpp>
#include <mrdocs/Extensions/ExtensionRegistry.hpp>
#include <mrdocs/Extensions/TransformExtensionContext.hpp>
#include <mrdocs/Support/Error/Assert.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Transform.hpp>
#include <algorithm>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

ExtensionRegistry::LibraryRegistry::LibraryRegistry() = default;
ExtensionRegistry::LibraryRegistry::~LibraryRegistry() = default;

ExtensionRegistry::ExtensionRegistry()
    : libraries_(std::make_shared<LibraryRegistry>())
    , generators_(std::make_unique<GeneratorRegistry>())
{
}

ExtensionRegistry::ExtensionRegistry(
    std::shared_ptr<LibraryRegistry> libraries)
    : libraries_(libraries ? std::move(libraries) : std::make_shared<LibraryRegistry>())
    , generators_(std::make_unique<GeneratorRegistry>())
{
}

ExtensionRegistry::~ExtensionRegistry() = default;

// The source of a move is left empty and valid only for destruction and
// assignment.
ExtensionRegistry::ExtensionRegistry(ExtensionRegistry&& other) noexcept
    : engines_(std::move(other.engines_))
    , loadedPaths_(std::move(other.loadedPaths_))
    , libraries_(std::move(other.libraries_))
    , generators_(std::move(other.generators_))
    , transforms_(std::move(other.transforms_))
{
    other.engines_.clear();
    other.loadedPaths_.clear();
    other.transforms_.clear();
}

// The members are released in the reverse of their declaration order, as the
// destructor does: the transforms and generators that call into a script
// engine go before the engines. A defaulted assignment would replace the
// engines first.
ExtensionRegistry&
ExtensionRegistry::operator=(ExtensionRegistry&& other) noexcept
{
    if (this != &other)
    {
        transforms_.clear();
        generators_ = std::move(other.generators_);
        engines_.clear();
        engines_ = std::move(other.engines_);
        loadedPaths_ = std::move(other.loadedPaths_);
        libraries_ = std::move(other.libraries_);
        transforms_ = std::move(other.transforms_);
        other.engines_.clear();
        other.loadedPaths_.clear();
        other.transforms_.clear();
    }
    return *this;
}

GeneratorRegistry&
ExtensionRegistry::generators() noexcept
{
    return *generators_;
}

GeneratorRegistry const&
ExtensionRegistry::generators() const noexcept
{
    return *generators_;
}

namespace {

// The `ctx` passed to a registered transform script.
//
// It is the described `TransformExtensionContext`, projected through
// `describedToDom`, so each field is a key:
//
// - `ctx.corpus`: the navigable corpus the script reads and mutates in place.
// - `ctx.config`: the generation configuration.
// - `ctx.params`: the `transform-options.<id>` block of the transform; an
//   empty object when unset.
//
// The binding adds `ctx.report` by hand (see `reportDom`). Any other field
// added to `TransformExtensionContext` is a key of `ctx` in every scripting
// language. The object copies `context`; the corpus, config and reporter it
// points to have to outlive it.
dom::Value
transformExtensionContextDom(TransformExtensionContext const& context)
{
    MRDOCS_ASSERT(context.report);
    dom::Object extras;
    extras.set("report", reportDom(*context.report));
    return dom::Value(
        dom::overlayObject(
            extras,
            dom::newObject<CachedDescribedObject<TransformExtensionContext>>(context)));
}

// A transform an extension script registered: the script's function, run
// with the `ctx` object the scripting API documents.
class ScriptTransform final
    : public Transform
{
    std::string id_;
    dom::Function function_;

public:
    ScriptTransform(std::string id, dom::Function function)
        : id_(std::move(id))
        , function_(std::move(function))
    {
    }

    std::string_view
    id() const noexcept override
    {
        return id_;
    }

    // A transform sees the described TransformExtensionContext as `ctx`:
    // `ctx.corpus` (per-symbol proxies plus `get(id)` / `lookup(name)`),
    // `ctx.config`, its own `ctx.params`, and `ctx.report`. Writes
    // (`ctx.corpus.symbols[i].name = "..."`) mutate the live Symbol through
    // the DOM's set path. The context is built for each call, and so is the
    // corpus object it projects: the symbol list of the corpus object is a
    // snapshot, so sharing one between transforms would let a script change
    // what the next one sees, or leave it a list that an earlier transform
    // has made out of date.
    Expected<void>
    apply(
        Corpus& corpus,
        Config const& config,
        report::Reporter& reporter) const override
    {
        TransformExtensionContext const context =
            makeTransformExtensionContext(*this, corpus, config, reporter);
        dom::Value const ctx = transformExtensionContextDom(context);
        Expected<dom::Value> const invoked = function_.tryInvoke(ctx);
        if (!invoked.has_value())
        {
            return Unexpected(Error(invoked.error()));
        }
        return {};
    }
};

// Run one script. A script that sits in a directory of its own can load the
// modules next to it, in the languages that have a way to.
Expected<LoadedExtensions>
loadExtensionsFromScript(
    ExtensionEntry const& entry,
    report::Reporter& reporter)
{
    switch (entry.kind)
    {
    case ExtensionKind::Lua:
        return loadLuaExtensions(entry.path, reporter, entry.directory);
    case ExtensionKind::JavaScript:
        return loadJsExtensions(entry.path, reporter);
    case ExtensionKind::Native:
        break;
    }
    // collectExtensions gives a script entry a script kind, so reaching
    // here would mean an internal mismatch.
    return Unexpected(formatError(
        "extension '{}': unsupported file extension", entry.path));
}

} // (anon)

Expected<ExtensionRegistry>
ExtensionRegistry::load(Config const& config, report::Reporter& reporter)
{
    ExtensionRegistry registry;
    MRDOCS_TRY(registry.loadExtensions(config, reporter));
    return registry;
}

Expected<void>
ExtensionRegistry::loadExtensions(
    Config const& config,
    report::Reporter& reporter)
{
    MRDOCS_ASSERT(libraries_);
    MRDOCS_TRY(std::vector<ExtensionEntry> entries,
        collectExtensions(config, reporter));

    for (ExtensionEntry const& entry : entries)
    {
        if (loadedPaths_.contains(entry.path))
        {
            continue;
        }
        if (entry.kind == ExtensionKind::Native)
        {
            MRDOCS_TRY(loadPlugin(
                entry.path, entry.directory, config, *this, *libraries_,
                reporter));
            loadedPaths_.insert(entry.path);
            continue;
        }
        MRDOCS_TRY(LoadedExtensions loaded,
            loadExtensionsFromScript(entry, reporter));
        // A refused script leaves nothing behind: its generator ids are
        // checked before its engine, transforms or path are recorded.
        std::set<std::string_view> ids;
        for (std::unique_ptr<Generator> const& g : loaded.generators)
        {
            if (generators_->find(g->id()) ||
                !ids.insert(g->id()).second)
            {
                return Unexpected(formatError(
                    "the script \"{}\" failed: generator id=\"{}\" "
                    "already exists",
                    entry.path, g->id()));
            }
        }
        for (std::unique_ptr<Generator>& g : loaded.generators)
        {
            if (Expected<void> added =
                    generators_->install(std::move(g));
                !added)
            {
                return Unexpected(formatError(
                    "the script \"{}\" failed: {}",
                    entry.path, added.error().reason()));
            }
        }
        loadedPaths_.insert(entry.path);
        engines_.push_back(std::move(loaded.vm));
        for (auto& [id, function] : loaded.transforms)
        {
            transforms_.push_back(std::make_shared<ScriptTransform>(
                std::move(id), std::move(function)));
        }
    }
    return {};
}

Expected<void>
ExtensionRegistry::installTransform(std::shared_ptr<Transform const> transform)
{
    MRDOCS_CHECK(transform, "cannot install a null transform");
    if (std::ranges::find(transforms_, transform) != transforms_.end())
    {
        return {};
    }
    transforms_.push_back(std::move(transform));
    return {};
}

Expected<void>
ExtensionRegistry::applyTransforms(
    Corpus& corpus,
    Config const& config,
    report::Reporter& reporter) const
{
    // Invoke each transform once, in the order it was added, stopping at
    // the first failure. A transform may have renamed symbols, so the
    // corpus drops its remembered lookups after each one, and the next
    // transform resolves names against what the previous ones wrote.
    Expected<void> result;
    for (std::shared_ptr<Transform const> const& transform : transforms_)
    {
        std::string const id(transform->id());
        result = transform->apply(corpus, config, reporter);
        corpus.invalidateLookupCache();
        if (!result)
        {
            result = Unexpected(formatError(
                "the transform \"{}\" failed: {}",
                id, result.error().reason()));
            break;
        }
    }
    return result;
}

} // mrdocs
