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

    Builds a fresh JS context, exposes the `mrdocs` global object,
    evaluates the script, and invokes `transform_corpus(corpus)` if
    defined. A script that defines no such function is silently
    skipped, so an empty `.js` file is valid.
*/
Expected<void>
runOneJsExtension(CorpusImpl& corpus, std::string const& scriptPath);

} // mrdocs

#endif
