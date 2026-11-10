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

#ifndef MRDOCS_API_EXTENSIONS_EXTENSIONREGISTRY_HPP
#define MRDOCS_API_EXTENSIONS_EXTENSIONREGISTRY_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Extensions/GeneratorRegistry.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <mrdocs/Transform.hpp>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

class Corpus;
class Config;

/** The pipeline extensions of one run, and the generators and transforms
    they add.

    A pipeline extension is an entry of an addon root's `extensions/`
    directory: a `.lua` or `.js` script, or a native shared library, either
    as a file directly in `extensions/` or as the entry file of a directory
    of its own. Running a script's top level (once, at @ref loadExtensions)
    registers two kinds of things: corpus transforms
    (`mrdocs.register_transform`) and output generators
    (`mrdocs.register_generator`). A native extension installs the same
    two kinds of things through its entry point (see `mrdocs/Plugin.hpp`).

    Extensions are optional and live entirely outside the @ref Corpus: a
    corpus is just extracted symbols. An application that has no extensions
    never needs to load any; it adds the generators and transforms it
    wants in its own code. An application that wants to honor a
    configuration's extensions loads them here and runs them as steps in
    its own workflow.

    The registry owns a @ref GeneratorRegistry, which it exposes as
    @ref generators. The generators of the run are in that registry
    whatever defined them, and the extensions add theirs to it as they
    load; the registry itself does not look generators up for anyone. It
    owns one pipeline of @ref Transform objects, whatever their origin.
    @ref loadExtensions adds the transforms of every extension, the native
    ones through the interface of a plugin and the scripts through
    `mrdocs.register_transform`. @ref applyTransforms runs the pipeline in
    the order the transforms were added. Extensions load in a stable
    order, and that order is not something a transform may rely on.

    The registry also owns the scripting engines and the registered
    functions, so it must outlive both @ref applyTransforms and any
    generator it hands out. Destroying the registry closes the engines,
    and a script may have kept the corpus it was handed, so a corpus given
    to @ref applyTransforms must outlive the registry: declare the corpus
    before the registry.

    The native libraries a process loads are recorded in a
    @ref LibraryRegistry, which a registry holds through a `shared_ptr`.
    A registry constructed alone has one of its own. A program that runs
    more than once in a process, such as a test runner, constructs one
    @ref LibraryRegistry and passes it to every registry, so that each
    library is loaded and its entry point run once.

    Typical workflow, after a corpus is built:

    @code
    // The corpus is declared before the registry, so it is destroyed after
    Corpus corpus = ...;
    ExtensionRegistry ext;
    MRDOCS_TRY(ext.loadExtensions(config, reporter));
    MRDOCS_TRY(ext.applyTransforms(corpus, config, reporter));
    Generator const* generator = ext.generators().find(id);
    @endcode

    An application that runs once can write
    `ExtensionRegistry::load(config, reporter)` instead of the
    `ExtensionRegistry` and `loadExtensions` lines. A program that wants
    the generators the addons define as data too, and the order the tool
    uses, constructs a @ref Session instead.
*/
class MRDOCS_DECL
    ExtensionRegistry
{
public:
    /** The record of the native libraries a process has loaded.

        A native extension is a shared library that the process keeps for
        as long as it runs, and its entry point runs once. This is the
        record of that: it remembers, for each library file it loaded, the
        transforms and generators the entry point installed, so that
        loading the same library again hands those over to the new
        @ref ExtensionRegistry instead of running the entry point a second
        time. The entry point of a plugin runs once per library registry,
        with the configuration of the run that loaded it.

        A registry constructed alone makes one of its own. A program that
        goes through several runs, such as a test runner, constructs one
        and shares it among them with
        @ref ExtensionRegistry(std::shared_ptr<LibraryRegistry>).

        The libraries are never unloaded, whether or not this object is
        destroyed: the code of a generator or transform a plugin installed
        has to stay mapped for as long as any registry holds it. A program
        that destroys it and constructs another one maps each library
        once, but runs its entry point again, with the configuration of
        that call; a plugin that keeps state between calls can fail.

        @par Thread Safety
        Loading is synchronized: concurrent calls that load the same
        library run its entry point once.
    */
    class MRDOCS_DECL
        LibraryRegistry
    {
    public:
        /** Constructor.

            The registry starts with no library loaded.
        */
        LibraryRegistry();

        /** Destructor.
        */
        ~LibraryRegistry();

        LibraryRegistry(LibraryRegistry const&) = delete;
        LibraryRegistry& operator=(LibraryRegistry const&) = delete;

        /** What the entry point of a library installed.

            This is the record kept for each library. A program has no use
            for it: the loader fills it in and reads it back.
        */
        struct MRDOCS_VISIBLE Record
        {
            /// The transforms it installed, in order.
            std::vector<std::shared_ptr<Transform const>> transforms;
            /// The generators it installed, in order.
            std::vector<std::shared_ptr<Generator const>> generators;
        };

    private:
        /** The loader opens libraries, runs entry points and records them.
        */
        friend Expected<void> loadPlugin(
            std::string const&,
            std::string const&,
            Config const&,
            ExtensionRegistry&,
            LibraryRegistry&,
            report::Reporter&);

        // Held while a library is looked up, opened, run and recorded, so
        // that concurrent loads of one library run its entry point once.
        std::mutex mutex_;
        // The libraries whose entry point has run, by canonical path.
        std::map<std::string, Record> byPath_;
    };

    /** Constructor.

        The registry starts with the generators that ship with MrDocs, no
        transforms, and a @ref LibraryRegistry of its own.
    */
    ExtensionRegistry();

    /** Constructor.

        The registry starts with the generators that ship with MrDocs and
        no transforms, and records the libraries it loads in `libraries`,
        which other registries may share.

        @param libraries The record of the loaded libraries. If null, the
        registry makes one of its own.
    */
    explicit ExtensionRegistry(std::shared_ptr<LibraryRegistry> libraries);

    /** Destructor.
    */
    ~ExtensionRegistry();

    /** Move constructor.

        The registry owns the scripting engines and the generators, so it
        is move-only. The source of a move can only be destroyed or
        assigned to.
    */
    ExtensionRegistry(ExtensionRegistry&&) noexcept;

    /** Move assignment.
    */
    ExtensionRegistry& operator=(ExtensionRegistry&&) noexcept;

    /** Return the generators of this registry.

        The extensions add theirs here as they load, and the built-in ones
        are here from the start.

        @return The generator registry this object owns.
    */
    GeneratorRegistry&
    generators() noexcept;

    /// @copydoc generators()
    GeneratorRegistry const&
    generators() const noexcept;

    /** Create a registry holding the extensions of a configuration.

        Equivalent to a new registry followed by @ref loadExtensions. It
        is meant for a program that loads the extensions of one
        configuration and then exits. A program that calls it again runs
        the entry point of each plugin again, with the configuration of
        that call; a plugin that keeps state between calls can fail. Such
        a program constructs one @ref LibraryRegistry and shares it among
        its registries.

        @param config The configuration whose addon roots are searched.
        @param reporter The reporter that prints what loading finds, such
        as a script that registers nothing.
        @return The loaded registry, or an error if an extension failed.
    */
    static Expected<ExtensionRegistry>
    load(Config const& config, report::Reporter& reporter);

    /** Discover and load the extensions declared by a configuration.

        Walks the `extensions/` directory of each addon root once, and
        loads what it finds. The order is stable for the same set of addon
        directories, but an extension must not rely on it. A script has its
        top level run once, so it can register its transforms and generators. A
        native library is loaded for the life of the process and its entry
        point is called, once per @ref LibraryRegistry. The transforms are
        appended to the pipeline, after whatever it already holds, and the
        generators are added to @ref generators.

        An extension this registry already loaded is not loaded again, so
        calling this twice, or after @ref load, adds nothing the second
        time. A native library that the @ref LibraryRegistry loaded for
        another registry hands its transforms and generators over without
        running its entry point again. The registry owns the engines and
        functions the scripts registered, and a script may keep the corpus
        it was handed, so any corpus given to @ref applyTransforms must
        outlive the registry.

        A generator that an extension, native or script, installs is
        refused when its id is already taken, by a built-in generator or by
        another extension, and the call fails naming the extension.

        An extension that cannot be loaded, whether a script that fails or
        a library that is missing, was built against another MrDocs, or
        reports an error of its own, fails the call: an extension is there
        because the user put it there, so one that does nothing is not
        silently accepted.

        @param config The configuration whose addon roots are searched.
        @param reporter The reporter that prints what loading finds, such
        as a script that registers nothing.
        @return The error, if an extension failed.
    */
    Expected<void>
    loadExtensions(Config const& config, report::Reporter& reporter);

    /** Append a transform to the pipeline.

        The transform runs after every transform added before it. Several
        transforms may share an id. A transform the pipeline already holds
        (the same object) is not added again, so adding it twice, for
        example by loading the same plugins twice into one registry, runs
        it once, at the position of the first addition.

        @param transform The transform to add. The registry shares
        ownership, so the same transform may be in the pipeline of several
        registries.
        @return An error if the transform is null.
    */
    Expected<void>
    installTransform(std::shared_ptr<Transform const> transform);

    /** Apply the pipeline of transforms to a corpus.

        Invokes every transform once, in the order it was added, and stops
        at the first one that fails. Each transform reads its own
        `transform-options` block with @ref getTransformParams. Run this after the corpus is built and
        finalized and before any generator runs, so the mutations are
        visible to every output format. The corpus forgets its remembered
        name lookups after each transform, so the next one resolves names
        against what the previous ones wrote.

        @param corpus The corpus to transform. It must outlive the
        registry, because a script may keep it.
        @param config The configuration each transform receives.
        @param reporter The reporter the transforms report through: their
        contexts carry it as `report`.
        @return The error, if any occurred, naming the transform it came
        from.
    */
    Expected<void>
    applyTransforms(
        Corpus& corpus,
        Config const& config,
        report::Reporter& reporter) const;

private:
    // One strong handle per loaded script to its engine (a type-erased
    // Context that keeps the VM alive). The transforms of a Lua script
    // and all the generators a script registered keep the engine alive
    // too. The transforms of a JavaScript script do not: their functions
    // point into the interpreter without owning it, so for those scripts
    // these handles are what keeps the engine running. They are declared
    // before transforms_ so the engine outlives the functions that use it.
    std::vector<std::shared_ptr<void>> engines_;
    // The extension files already loaded, so that loading them again is a
    // no-op.
    std::set<std::string> loadedPaths_;
    // The record of the native libraries, possibly shared with other
    // registries.
    std::shared_ptr<LibraryRegistry> libraries_;
    // The generators of every origin. They may call into an engine, so
    // they are declared after engines_. The registry is not movable, so it
    // is held by pointer to keep this class movable.
    std::unique_ptr<GeneratorRegistry> generators_;
    std::vector<std::shared_ptr<Transform const>> transforms_;
};

} // mrdocs

#endif // MRDOCS_API_EXTENSIONS_EXTENSIONREGISTRY_HPP
