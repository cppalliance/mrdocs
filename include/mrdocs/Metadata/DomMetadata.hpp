//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_DOM_DOMMETADATA_HPP
#define MRDOCS_API_DOM_DOMMETADATA_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata.hpp>
#include <type_traits>
#include <memory>

namespace clang {
namespace mrdocs {

/** Front-end factory for producing Dom nodes.

    A @ref Generator subclasses this object and
    then uses it to create the Dom nodes used for
    rendering in template engines.
*/
class MRDOCS_DECL
    DomCorpus
{
    class Impl;

    std::unique_ptr<Impl> impl_;

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

    /** Construct a Dom object representing the given symbol.

        This function is called internally when a `dom::Object`
        representing a symbol needs to be constructed because
        it was not found in the cache.
    */
    virtual
    dom::Object
    construct(Info const& I) const;

    /** Return a Dom object representing the given symbol.

        @return A value containing the symbol contents.

        @param id The id of the symbol to return.
    */
    dom::Object
    get(SymbolID const& id) const;

    /** Return a Dom object representing the given symbol.

        When `id` is zero, this function returns null.

        @return A value containing the symbol
        contents, or null if `id` equals zero.

        @param id The id of the symbol to return.
    */
    dom::Value
    getOptional(
        SymbolID const& id) const;

    /** Return a Dom value representing the Javadoc.

        The default implementation returns null. A
        @ref Generator should override this member
        and return a value that has suitable strings
        in the generator's output format.
    */
    virtual
    dom::Value
    getJavadoc(
        Javadoc const& jd) const;
};

} // mrdocs
} // clang

#endif
