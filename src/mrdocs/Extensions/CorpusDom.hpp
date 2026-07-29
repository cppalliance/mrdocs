//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_EXTENSIONS_CORPUSDOM_HPP
#define MRDOCS_LIB_EXTENSIONS_CORPUSDOM_HPP

#include <mrdocs/Dom.hpp>
#include <string_view>

namespace mrdocs {

class Corpus;
class Config;

/** Build the `ctx.corpus` object seen by extension scripts.

    The returned value is a small object:

    - `corpus.symbols` -- a `DescribedArrayProxy` over the live
      symbol vector. Reads through reflection on the concrete C++
      type; writes are gated on the allowlist for top-level Symbol
      fields. Push/replace propagate to the underlying vector.
    - `corpus.get(id)` -- decode a base58 id string and return the
      proxy for that symbol, or `null` if no symbol has that id.
    - `corpus.lookup(name)` -- look up a symbol by name in the global
      namespace (mirrors `Corpus::lookup(name)`). Returns the proxy
      or `null`.
*/
dom::Value
buildCorpusDom(Corpus& corpus);

/** Build the `ctx` argument passed to one registered corpus transform.

    A transform receives one object so new capabilities can be added
    without changing its signature:

    - `ctx.corpus` -- the navigable corpus (see @ref buildCorpusDom) the
      transform reads and mutates in place.
    - `ctx.config` -- the generation configuration.
    - `ctx.params` -- the transform's own `transform-options.<id>` block,
      keyed by the id it registered under; an empty object when unset.

    The corpus DOM is built once per script (it is `O(symbols)`) and passed
    in as `corpusDom`, so the per-transform context is cheap to assemble.
*/
dom::Value
buildTransformContext(
    dom::Value const& corpusDom, Config const& config, std::string_view id);

} // mrdocs

#endif
