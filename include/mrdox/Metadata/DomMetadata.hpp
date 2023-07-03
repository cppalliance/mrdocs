//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_DOMMETADATA_HPP
#define MRDOX_API_DOM_DOMMETADATA_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Support/Dom.hpp>
#include <mrdox/Metadata.hpp>
#include <type_traits>
#include <memory>

namespace clang {
namespace mrdox {

/** Front-end factory for producing Dom nodes.

    A @ref Generator subclasses this object and
    then uses it to create the Dom nodes used for
    rendering in template engines.
*/
class MRDOX_DECL
    DomCorpus
{
    class Impl;

    std::unique_ptr<Impl> impl_;

public:
    /** The Corpus associated with the Dom.
    */
    Corpus const& corpus;

    /** Destructor.
    */
    virtual ~DomCorpus();

    /** Constructor.

        Ownership of the specified Corpus is not
        transferred; the caller is responsible for
        ensuring the lifetime extension of the object.

        @param corpus The Corpus whose metadata to use.
    */
    explicit
    DomCorpus(Corpus const& corpus);

    /** Return a Dom object representing the given symbol.

        @return A value containing the symbol contents.

        @param id The id of the symbol to return.
    */
    dom::Object
    get(SymbolID const& id) const;

    /** Return a Dom object representing the given symbol.

        @return A value containing the symbol contents.

        @param I The metadata for the symbol.
    */
    dom::Object
    get(Info const& I) const;

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

} // mrdox
} // clang

#endif
