//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_EXTENSIONS_EXTENSIONCONTEXT_HPP
#define MRDOCS_LIB_EXTENSIONS_EXTENSIONCONTEXT_HPP

#include <mrdocs/Dom.hpp>
#include <string_view>

namespace mrdocs {

class Corpus;
class Config;

/** Build the context object passed to a registered extension script.

    Every extension kind receives one object, so new capabilities can be
    added without changing its signature:

    - `ctx.corpus` -- the navigable corpus (see @ref buildCorpusDom) the
      script reads and, for a transform, mutates in place.
    - `ctx.config` -- the generation configuration.
    - `ctx.params` -- the script's own options block, keyed by the `id` it
      registered under; an empty object when unset.

    `corpus` and `config` are the same for every extension kind. `params`
    is looked up by `id`; transforms are the only kind that reads options
    today (from `transform-options.<id>`), and other kinds pass an id with
    no options and get the empty object.

    The corpus DOM is built once per script (it is `O(symbols)`) and
    passed in as `corpusDom`, so the per-script context is cheap to build.
*/
dom::Value
buildExtensionContext(
    dom::Value const& corpusDom, Config const& config, std::string_view id);

} // mrdocs

#endif
