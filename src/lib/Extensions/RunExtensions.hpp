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

    Extensions are discovered under each addon root's extensions/
    directory (the primary addons plus addons-supplemental): a .lua or
    .js file is an extension. Each script declares corpus transforms by
    calling `register_transform(fn)`; every registered function is
    invoked once, in registration order, with a navigable DOM view of the
    corpus. A transform reads the corpus through that view and mutates it
    by assigning to symbol fields (for example `sym.name = "..."`), which
    writes through to the live symbol.

    Any uncaught error inside a script aborts the build. Scripts run in
    alphabetical order by full path, with the two languages interleaved so
    behavior doesn't depend on which language a user chose. Extensions
    fire after all finalizers and before any generator runs, so mutations
    are visible to every output format.
*/
Expected<void>
runExtensions(CorpusImpl& corpus);

} // mrdocs

#endif
