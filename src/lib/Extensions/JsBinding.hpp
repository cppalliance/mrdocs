//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_EXTENSIONS_JSBINDING_HPP
#define MRDOCS_LIB_EXTENSIONS_JSBINDING_HPP

#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <string>

namespace mrdocs {

class CorpusImpl;

/** Run one JavaScript extension script against the corpus.

    Build a fresh JS context and evaluate the script. The script declares
    corpus transforms with `mrdocs.register_transform(fn)` and output
    generators with `mrdocs.register_generator(id, fn)`, in either
    combination. Each transform is invoked once, in registration order,
    with a navigable DOM view of the corpus it can read and mutate in
    place; each generator is handed to the
    corpus to run later, once one is selected. A script that registers
    nothing causes a warning and otherwise has no effect, so an empty .js
    file is tolerated.
*/
Expected<void>
runOneJsExtension(CorpusImpl& corpus, std::string const& scriptPath);

} // mrdocs

#endif
