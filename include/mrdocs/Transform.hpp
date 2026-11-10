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

// A pass over the corpus, run between extraction and generation.

#ifndef MRDOCS_API_TRANSFORM_HPP
#define MRDOCS_API_TRANSFORM_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Support/Error/Error.hpp>
#include <mrdocs/Support/Error/Expected.hpp>
#include <string_view>

namespace mrdocs {

/** Base class for corpus transforms.

    A transform runs once, after the corpus is built and finalized and before
    any generator runs, so whatever it changes is what every output format
    sees. It is handed the corpus itself rather than a copy, and may read it,
    change the symbols it finds, or both. It cannot create or destroy a symbol,
    and what it changes has to leave the corpus in the shape finalization left
    it in. A symbol keeps its id and kind, every symbol ID written into the
    corpus names a symbol the corpus has, and nothing finalization derived is
    derived again. The Corpus Transforms page of the documentation lists these
    rules in full.

    An @ref ExtensionRegistry collects transforms and runs them as one
    pipeline. The transforms a plugin installs through @ref
    PluginContext::registerTransform and the ones an extension script registers
    go through the same list. Extensions run in a stable order, but a
    transform must not rely on it. A transform works on whatever corpus it is
    handed and must not assume another transform ran before it or will run
    after it.
*/
class MRDOCS_DECL
    Transform
{
public:
    /** Destructor.
    */
    virtual ~Transform() noexcept;

    /** Return the symbolic name of the transform.

        A diagnostic about a transform names it with this, so a
        recognizable name is worth choosing. The id also keys the
        `transform-options` block @ref getTransformParams returns (declared
        in `mrdocs/Extensions/TransformExtensionContext.hpp`), so
        transforms that share an id share their parameters. Unlike a
        generator id it does not choose which transform runs, and need
        not be unique.
    */
    virtual std::string_view
    id() const noexcept = 0;

    /** Transform the corpus.

        A transform reports through `reporter`, the reporter of the session
        that runs it.

        @par Thread Safety
        The transforms of one pipeline run one at a time, in the order
        they were added. A transform that several registries hold can run
        concurrently, for different corpora.

        @return The error, if any occurred. An error stops the run,
        before any generator is given the corpus.

        @param corpus The corpus to read and change.
        @param config The configuration that drove the build. A transform
        that has options reads them with @ref getTransformParams, declared in
        `mrdocs/Extensions/TransformExtensionContext.hpp`.
        @param reporter The reporter of the run.
    */
    virtual Expected<void>
    apply(
        Corpus& corpus,
        Config const& config,
        report::Reporter& reporter) const = 0;
};

} // mrdocs

#endif // MRDOCS_API_TRANSFORM_HPP
