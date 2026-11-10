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

#ifndef MRDOCS_API_EXTENSIONS_GENERATORREGISTRY_HPP
#define MRDOCS_API_EXTENSIONS_GENERATORREGISTRY_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Generator.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace mrdocs {

/** The generators a run can select.

    The registry owns every @ref Generator of a run, whatever defined it. That
    covers the ones that ship with MrDocs (`adoc`, `html`, `xml`, `json` and
    `noop`), the ones the addons contribute as data, and the ones that
    pipeline extensions install. It starts with the built-in ones. It does not
    know where the others come from. A script, a plugin, a data-driven
    definition and a program's own code all add through @ref install.
    There is no process-wide registry, so two registries in one process do not
    see each other's generators.

    @code
    GeneratorRegistry generators;
    MRDOCS_TRY(generators.install(std::make_shared<MyGenerator>()));
    Generator const* generator = generators.find("my-format");
    @endcode

    @par Thread Safety
    Adding and looking up a generator are synchronized with each other.
*/
class MRDOCS_DECL
    GeneratorRegistry
{
public:
    /** Constructor.

        The registry starts with the generators that ship with MrDocs.
    */
    GeneratorRegistry();

    /** Destructor.
    */
    ~GeneratorRegistry();

    /** Copy and move are deleted.

        The generators of a registry may call into the script engines of
        the @ref ExtensionRegistry that owns it, so they must not outlive
        it: nothing moves a registry out of its owner.
    */
    GeneratorRegistry(GeneratorRegistry const&) = delete;
    GeneratorRegistry& operator=(GeneratorRegistry const&) = delete;

    /** Install a generator.

        An id belongs to one generator, whoever registers it. The registry
        shares ownership of the generator. A generator the registry already
        holds (the same object) is not added again, which is what makes
        loading the same plugin into one registry twice harmless.

        @par Thread Safety
        The call is synchronized with the other calls that add or look up
        a generator of this registry.

        @param generator The generator to add.
        @return An error if the generator is null, or if another
        generator already has its id.
    */
    Expected<void>
    install(std::shared_ptr<Generator const> generator);

    /** Return the generators, in the order they were added.

        @return A pointer to each generator: the ones that ship with
        MrDocs first, then the ones added later.
    */
    std::vector<Generator const*>
    generators() const;

    /** Return the generator with this id, or `nullptr`.

        @param id The generator id to look up. It must match exactly,
        including case.
        @return The generator, or `nullptr` if none has the id.
    */
    Generator const*
    find(std::string_view id) const noexcept;

private:
    // Guards generators_.
    mutable std::mutex mutex_;
    std::vector<std::shared_ptr<Generator const>> generators_;
};

} // mrdocs

#endif // MRDOCS_API_EXTENSIONS_GENERATORREGISTRY_HPP
