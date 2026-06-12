//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_SCRIPT_SCRIPTGENERATOR_HPP
#define MRDOCS_LIB_GEN_SCRIPT_SCRIPTGENERATOR_HPP

#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <string_view>

namespace mrdocs::script {

/** Run a script-defined output generator.

    A generator declared with `register_generator(id, fn)` is a
    `dom::Function` that owns the whole emit. Invoke it with one `ctx`
    object, mirroring the shape a transform receives:

    @li `ctx.corpus` is the read-only corpus, a `symbols` array of the
        same per-symbol objects the templates see, each tagged with its
        flat `_id` so the generator can form stable per-symbol URLs.

    @li `ctx.output` exposes `write(path, contents)`, resolved under the
        output directory; a path that escapes it is rejected.

    @li `ctx.config` is the resolved configuration, as templates see it.

    The function is language-agnostic: a `dom::Function` self-owns its
    scripting VM, so one call drives a Lua or a JavaScript generator
    without the host knowing which.

    @param generate The registered generator function.
    @param id The generator id, used to tag diagnostics.
    @param corpus The finalized corpus to emit.
    @param outputPath The output directory the generator writes under.
*/
Expected<void>
runScriptGenerator(
    dom::Function const& generate,
    std::string_view id,
    Corpus const& corpus,
    std::string_view outputPath);

} // mrdocs::script

#endif
