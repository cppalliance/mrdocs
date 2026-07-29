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
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

class Corpus;
class Config;

/** The user extension scripts declared by a configuration, loaded.

    Extensions are optional and live entirely outside the @ref Corpus: a
    corpus is just extracted symbols. An application that has no extensions
    never needs this type; it does whatever an extension would do in its
    own code. An application that wants to honor a configuration's
    extension scripts loads them here and runs them as steps in its own
    workflow.

    A `.lua` or `.js` file under an addon root's `extensions/` directory is
    an extension. Running its top level (once, at @ref load) registers two
    kinds of things: corpus transforms (`mrdocs.register_transform`) and
    output generators (`mrdocs.register_generator`). This object owns the
    scripting engines and the registered functions, so it must outlive both
    @ref applyTransforms and any generator it hands out.

    Typical workflow, after a corpus is built:

    @code
    MRDOCS_TRY(ExtensionRegistry ext, ExtensionRegistry::load(config));
    MRDOCS_TRY(ext.applyTransforms(corpus, config));
    // then run generators: ext.findGenerator(id), else the built-in registry
    @endcode
*/
class MRDOCS_VISIBLE
    ExtensionRegistry
{
public:
    /** Destructor.
    */
    MRDOCS_DECL ~ExtensionRegistry();

    /** Move constructor.

        The registry owns the scripting engines and the generators loaded
        from them, so it is move-only.
    */
    MRDOCS_DECL ExtensionRegistry(ExtensionRegistry&&) noexcept;

    /** Move assignment.
    */
    MRDOCS_DECL ExtensionRegistry& operator=(ExtensionRegistry&&) noexcept;

    /** Discover and load the extension scripts declared by a configuration.

        Walks each addon root's `extensions/` directory, in a stable order,
        and runs each script's top level once so it can register its
        transforms and generators. Owns the engines and functions the
        scripts registered.

        @param config The configuration whose addon roots are searched.
        @return The loaded registry, or an error if a script failed.
    */
    MRDOCS_DECL
    static
    Expected<ExtensionRegistry>
    load(Config const& config);

    /** Apply the registered transforms to a corpus.

        Invokes every transform once, in registration order, with a
        navigable DOM view of the corpus it reads and mutates in place. Run
        this after the corpus is built and finalized and before any
        generator runs, so the mutations are visible to every output
        format.

        @param corpus The corpus to transform.
        @param config The configuration (supplies each transform's params).
        @return The error, if any occurred.
    */
    MRDOCS_DECL
    Expected<void>
    applyTransforms(Corpus& corpus, Config const& config) const;

    /** Return the script-defined generators, as @ref Generator objects.

        @return A pointer to each registered generator; empty when the
        loaded scripts registered none.
    */
    MRDOCS_DECL
    std::vector<Generator const*>
    generators() const;

    /** Return the script-defined generator with this id, or `nullptr`.

        @param id The generator id to look up.
        @return The generator, or `nullptr` if none is registered.
    */
    MRDOCS_DECL
    Generator const*
    findGenerator(std::string_view id) const noexcept;

private:
    ExtensionRegistry();

    // One loaded script: a strong handle to its engine (a type-erased
    // Context that keeps the VM alive) and its registered transforms.
    struct Script
    {
        std::shared_ptr<void> vm;
        std::vector<std::pair<std::string, dom::Function>> transforms;
    };

    std::vector<Script> scripts_;
    std::vector<std::unique_ptr<Generator>> generators_;
};

} // mrdocs

#endif // MRDOCS_API_EXTENSIONS_EXTENSIONREGISTRY_HPP
