//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOMCORPUS_HPP
#define MRDOCS_API_METADATA_DOMCORPUS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace mrdocs {

class Corpus;
struct Symbol;
struct DocComment;
class SymbolID;

/** Front-end factory for producing Dom nodes.

    This class keeps a reference to the @ref Corpus
    of extracted metadata, and provides a mechanism
    for constructing DOM nodes representing the metadata.

    A @ref Generator can subclass this object
    then uses it to create the Dom nodes used
    as input for rendering template engines.
*/
class MRDOCS_DECL
    DomCorpus
{
    Corpus const& corpus_;

public:
    /** Destructor.
    */
    virtual ~DomCorpus();

    /** Constructor.

        Ownership of the specified Corpus is not
        transferred; the caller is responsible for
        ensuring the lifetime extension of the object.

        @param corpus The Corpus whose metadata to use.
    */
    DomCorpus(Corpus const& corpus);

    /** Returns the Corpus associated with the Dom.
    */
    Corpus const& getCorpus() const;

    /** Returns the Corpus associated with the Dom.
    */
    Corpus const& operator*() const
    {
        return getCorpus();
    }

    /** Returns the Corpus associated with the Dom.
    */
    Corpus const* operator->() const
    {
        return &getCorpus();
    }

    /** Construct the Dom object representing the specified symbol.

        The object is a pure reflection view of the concrete C++ symbol:
        its fields come from `describe`, cross-references stay as their
        reflected SymbolID strings (a template resolves them explicitly,
        e.g. via a `lookup_symbol` helper), and there are no computed
        "exception" fields. The view is read-only.

        Called internally when a symbol's `dom::Object` is needed and was
        not found in the cache.
    */
    virtual
    dom::Object
    construct(Symbol const& I) const;

    /** Return a Dom object representing the given symbol.

        @return A value containing the symbol
        contents, or null if `id` is invalid.

        @param id The id of the symbol to return.
    */
    dom::Value
    get(SymbolID const& id) const;
};

/** Build a DOM object representing an entire corpus.

    The returned object is the single, shared navigable view of the
    corpus used by both the Handlebars generators (exposed as
    `@root.mrdocs.corpus`) and extension scripts (exposed as
    `ctx.corpus`). It exposes:

    @li `corpus.symbols` -- a lazy array over every symbol; each element
        materializes a reflection proxy for that symbol on access.
    @li `corpus.get id` -- decode a base58 id string and return that
        symbol's proxy, or null.
    @li `corpus.lookup name` -- look up a symbol by qualified name from
        the global namespace, or null. A second argument (a symbol object
        or a base58 id) resolves `name` relative to that scope instead.

    This overload yields mutable symbol proxies: writes go straight to
    the reflected fields (the extension path).

    @param corpus The corpus to view.
    @return A DOM object exposing the corpus.
*/
dom::Value buildCorpusDom(Corpus& corpus);

/** Build a DOM object representing an entire corpus.

    Read-only overload: it yields read-only symbol proxies (the
    generator render path). See @ref buildCorpusDom(Corpus&) for the
    shape of the returned object.

    @param corpus The corpus to view.
    @return A DOM object exposing the corpus.
*/
dom::Value buildCorpusDom(Corpus const& corpus);

} // mrdocs

#endif // MRDOCS_API_METADATA_DOMCORPUS_HPP
