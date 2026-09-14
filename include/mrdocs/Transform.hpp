//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
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
#include <memory>
#include <string_view>

namespace mrdocs {

/** Base class for corpus transforms.

    A transform runs once, after the corpus is built and finalized and
    before any generator runs, so whatever it changes is what every
    output format sees. It is handed the corpus itself rather than a
    copy, and may read it, change the symbols it finds, or both. What
    it cannot do is create a symbol or destroy one, since a corpus keeps
    its storage to itself.
*/
class MRDOCS_VISIBLE
    Transform
{
public:
    /** Destructor.
    */
    MRDOCS_DECL
    virtual
    ~Transform() noexcept;

    /** Return the symbolic name of the transform.

        A diagnostic about a transform names it with this, so a
        recognizable name is worth choosing. Unlike a generator id it
        selects nothing, and need not be unique.
    */
    MRDOCS_DECL
    virtual
    std::string_view
    id() const noexcept = 0;

    /** Transform the corpus.

        @par Thread Safety
        Transforms run one at a time, in the order they were installed.

        @return The error, if any occurred. An error stops the run,
        before any generator is given the corpus.

        @param corpus The corpus to read and change.
        @param config The configuration that drove the build.
    */
    MRDOCS_DECL
    virtual
    Expected<void>
    apply(Corpus& corpus, Config const& config) const = 0;
};

/** Install a corpus transform.

    This function registers a transform with the global transform
    registry, so that it runs on the corpus of the current build.

    A plugin installs its transforms through
    @ref PluginContext::installTransform, which calls this function.

    @par Thread Safety
    This function is thread-safe and may be called concurrently from
    multiple threads.

    @return An error if the transform is null.

    @param T The transform to install. Ownership is transferred to the
    registry.
*/
MRDOCS_DECL
Expected<void>
installTransform(std::unique_ptr<Transform> T);

/** Apply the installed transforms to a corpus.

    Invokes each installed transform once, in the order the transforms
    were installed, and stops at the first one that fails.

    Call this after the corpus is finalized and before a generator runs.
    It is one of the pieces the command-line tool composes to run its
    generate step; the order of that step lives in the tool.

    @par Thread Safety
    Safe against a concurrent @ref installTransform, which the registry
    synchronizes; a transform installed while this runs simply does not
    run this time.

    @return The error, if any occurred, naming the transform it came
    from.

    @param corpus The corpus to transform.
    @param config The configuration that drove the build.
*/
MRDOCS_DECL
Expected<void>
applyTransforms(Corpus& corpus, Config const& config);

} // mrdocs

#endif // MRDOCS_API_TRANSFORM_HPP
