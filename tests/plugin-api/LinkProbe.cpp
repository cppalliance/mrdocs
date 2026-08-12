//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

// Call the public API from outside the tool, the way a plugin does, so that
// building this file checks that the API is exported. A plugin compiles its
// own copy of every inline and template member, and those copies call
// out-of-line functions that need an export attribute of their own; a
// missing one is an unresolved external here, and nothing else in the tree
// would notice. Sorting the members is what earns the traversal its place:
// it compares every symbol kind, so it reaches far more of the API than it
// names.
//
// Nothing runs, and there is no ctest entry: the object is linked whole, so
// every symbol it names has to resolve. On Windows that resolution is
// against the tool's export table, which is where a missing attribute
// would fail. Elsewhere a module library may leave symbols to the loader,
// so what this checks there is that the public headers compile with neither
// `MRDOCS_TOOL` nor `MRDOCS_STATIC_LINK` defined, and that such a library
// links against the executable at all.

#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Transform.hpp>
#include <cstddef>
#include <memory>
#include <string>

using namespace mrdocs;

std::size_t
probeTraversal(Corpus const& corpus, NamespaceSymbol const& I)
{
    Corpus::TraverseOptions opts;
    opts.ordered = true;
    opts.recursive = true;
    std::size_t count = corpus.size();
    corpus.traverse(opts, I, [&count](auto const&) { ++count; });
    for ([[maybe_unused]] Symbol const& J : corpus)
    {
        ++count;
    }
    return count;
}

std::string
probeNames(Corpus const& corpus, SymbolID const& id)
{
    Symbol const& I = corpus.get(id);
    return corpus.qualifiedName(I) + corpus.qualifiedName(I, id);
}

Expected<Symbol const&>
probeLookup(Corpus const& corpus)
{
    return corpus.lookup("x");
}

namespace {

class ProbeTransform final
    : public Transform
{
public:
    std::string_view
    id() const noexcept override
    {
        return "probe";
    }

    Expected<void>
    apply(Corpus& corpus, Config const&) const override
    {
        Expected<void> result;
        if (corpus.find(SymbolID::global) == nullptr)
        {
            result = Unexpected(formatError("the global namespace is missing"));
        }
        return result;
    }
};

} // (anon)

Expected<void>
probeInstallTransform()
{
    return installTransform(std::make_unique<ProbeTransform>());
}
