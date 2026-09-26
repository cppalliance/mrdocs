//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_FILTEREDSYMBOLFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_FILTEREDSYMBOLFINALIZER_HPP

#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Symbol.hpp>
#include <mrdocs/Metadata/TArg.hpp>
#include <mrdocs/Metadata/Type.hpp>

namespace mrdocs {

class Corpus;
class Config;

/** Reports filtered symbols that are named by a documented declaration.

    The filters leave a symbol without a page, so a signature naming it
    sends the reader nowhere. The author can widen the filters, change
    the signature, or mark the symbol `@implementationdefined`, which
    prints a placeholder in place of the name and ends the report.

    Only the project's own symbols are reported, which is to say those
    declared in one of the inputs. A symbol from anywhere else, such as a
    standard library installed under `source-root`, belongs to someone
    else and gives the author nothing to answer.
*/
class FilteredSymbolFinalizer
{
    Corpus& corpus_;
    Config const& config_;

    void
    reportIfFiltered(Symbol const& referrer, Polymorphic<Type> const& type);

    void
    reportIfFiltered(Symbol const& referrer, Name const& name);

public:
    FilteredSymbolFinalizer(Corpus& corpus, Config const& config)
        : corpus_(corpus)
        , config_(config)
    {}

    /** Report every filtered symbol named by a documented declaration. */
    void
    build();
};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_FILTEREDSYMBOLFINALIZER_HPP
