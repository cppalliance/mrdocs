//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_FUNCTIONOBJECTFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_FUNCTIONOBJECTFINALIZER_HPP

#include <lib/CorpusImpl.hpp>
#include <mrdocs/Metadata/Symbol/Namespace.hpp>
#include <mrdocs/Metadata/Symbol/Record.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Metadata/Symbol/Variable.hpp>
#include <vector>

namespace mrdocs {

/** Identifies function object variables and transforms their metadata.

    A function object is a variable whose type is a record that provides
    operator() overloads as its only public non-special members. This
    finalizer detects such patterns (via auto-detection or the @functionobject
    doc command), hides the type as implementation-defined, and associates the
    operator() overloads with the variable so it can be documented as a callable.
*/
class FunctionObjectFinalizer
{
    CorpusImpl& corpus_;

    /** Check if a record qualifies as a function object type via
        auto-detection: its only public non-special members are
        operator() overloads.

        @param allowTemplates When true, class templates are allowed.
               Otherwise, class templates are excluded from auto-detection
               to avoid false positives like std::hash<T>.
    */
    bool
    isFunctionObjectType(
        RecordSymbol const& R,
        bool allowTemplates = false) const;

    /** Check if @p ancestor appears in the parent chain of @p descendant.

        Returns true when @p ancestor is the direct parent, grandparent,
        or any further ancestor of @p descendant. This is used to decide
        whether a class template should be considered for auto-detection:
        templates whose type lives in an enclosing scope of the
        variable (including sub-namespaces like `detail`) are allowed,
        while types from unrelated scopes (e.g. `std::`) are not.
    */
    bool
    isEnclosingScope(
        SymbolID ancestor,
        SymbolID descendant) const;

    /** Collect operator() FunctionSymbol IDs from a record's public
        interface.
    */
    std::vector<SymbolID>
    findCallOperatorOverloads(RecordSymbol const& R) const;

    /** Mark a record and all its children as implementation-defined.
    */
    void
    markImplementationDefined(RecordSymbol& R);

    /** Process variables in a namespace, transforming function objects.
    */
    void
    processNamespace(NamespaceSymbol& NS);

    /** Process variables in a record, transforming function objects.
    */
    void
    processRecord(RecordSymbol& R);

    /** Core logic: check a variable and transform it if it qualifies.

        @return true if the variable was transformed into a function object.
    */
    bool
    processVariable(
        VariableSymbol& V,
        std::vector<SymbolID>& sourceList,
        std::vector<SymbolID>& targetList);

public:
    FunctionObjectFinalizer(CorpusImpl& corpus)
        : corpus_(corpus)
    {}

    void
    build();
};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_FUNCTIONOBJECTFINALIZER_HPP
