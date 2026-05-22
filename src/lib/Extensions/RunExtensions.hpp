//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_EXTENSIONS_RUNEXTENSIONS_HPP
#define MRDOCS_LIB_EXTENSIONS_RUNEXTENSIONS_HPP

#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>

namespace mrdocs {

class CorpusImpl;

/** Run user-provided extension scripts against the corpus.

    Extensions live in <addon>/extensions/<name>.{lua,js} for each
    addon root declared in the configuration (primary `addons` plus
    `addons-supplemental`). Each script may export a function named
    `transform_corpus(corpus)`; the function is invoked once with a flat
    DOM view of the corpus that the script can read, and may mutate the
    corpus by calling functions on the pre-registered `mrdocs` global
    table or object:

    - `mrdocs.set(symbol_id, field, value)` - assign a new value to
      one of the allowlisted fields of a symbol. The setter validates
      its arguments and raises an error on misuse.

    Any uncaught error inside a script aborts the build. Scripts are run
    in alphabetical order by file path, with the two languages
    interleaved so behavior doesn't depend on which language a user
    chose. Extensions intentionally fire after all finalizers and
    before any generator runs, so mutations are visible to every
    output format.
*/
Expected<void>
runExtensions(CorpusImpl& corpus);

} // mrdocs

#endif
