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
#include <string_view>

namespace mrdocs {

/** Base class for corpus transforms.

    A transform runs once, after the corpus is built and finalized and
    before any generator runs, so whatever it changes is what every
    output format sees. It is handed the corpus itself rather than a
    copy, and may read it, change the symbols it finds, or both. It
    cannot create a symbol or destroy one, and what it changes has to
    leave the corpus in the shape finalization left it in: a symbol
    keeps its id and kind, every symbol ID written into the corpus names
    a symbol the corpus has, and nothing finalization derived is derived
    again. The Plugins page of the documentation lists these rules in
    full.

    A plugin installs a transform through
    @ref PluginContext::installTransform.
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

} // mrdocs

#endif // MRDOCS_API_TRANSFORM_HPP
