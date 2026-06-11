//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_EXTENSIONS_LUABINDING_HPP
#define MRDOCS_LIB_EXTENSIONS_LUABINDING_HPP

#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <string>

namespace mrdocs {

class CorpusImpl;

/** Run one Lua extension script against the corpus.

    Build a fresh Lua context, evaluate the script, and run every corpus
    transform it declares by calling `register_transform(fn)`. Each
    registered function is invoked once, in registration order, with a
    navigable DOM view of the corpus that it can read and mutate in place.
    A script that registers nothing causes a warning and otherwise has no
    effect, so an empty .lua file is tolerated.
*/
Expected<void>
runOneLuaExtension(CorpusImpl& corpus, std::string const& scriptPath);

} // mrdocs

#endif
