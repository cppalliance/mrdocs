//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_INTERFACE_HPP
#define MRDOCS_API_METADATA_INTERFACE_HPP

#include <memory>
#include <vector>
#include <mrdocs/MetadataFwd.hpp>
#include <mrdocs/Platform.hpp>

namespace clang::mrdocs {

/** A group of children that have the same access specifier.

    This struct represents a collection of symbols that share
    the same access specifier within a scope.

    It includes one vector for each info type,
    and individual vectors for static functions, types,
    and overloads.

    The tranche is not part of the Corpus. It is a temporary
    structure generated to aggregate the symbols of a scope. This
    structure is provided to the user via the DOM.
*/
struct Tranche
{
    #define INFO(Type) std::vector<SymbolID> Type;
    #include <mrdocs/Metadata/InfoNodesPascalPlural.inc>

    /// The types with the same access specifier in a scope.
    std::vector<SymbolID> Types;

    /// The static functions with the same access specifier in a scope.
    std::vector<SymbolID> StaticFunctions;

    /// The overloads with the same access specifier in a scope.
    ScopeInfo Overloads;

    /// The static overloads with the same access specifier in a scope.
    ScopeInfo StaticOverloads;
};

/** Return a tranche representing the members of a namespace.

    @return The tranche.

    @param Namespace The namespace to build the tranche for.
    @param corpus The complete metadata.
*/
MRDOCS_DECL
Tranche
makeTranche(
    NamespaceInfo const& Namespace,
    Corpus const& corpus);

/** Return the Tranche as a @ref dom::Value object.
 */
MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    std::shared_ptr<Tranche> const& sp,
    DomCorpus const* domCorpus);

/** The aggregated interface for a given struct, class, or union.

    This class represents the public, protected, and private
    interfaces of a record. It is used to generate the
    "interface" value of the DOM for symbols that represent
    records or namespaces.

    The interface is not part of the Corpus. It is a temporary
    structure generated to aggregate the symbols of a record.
    This structure is provided to the user via the DOM.

    While the members of a Namespace are directly represented
    with a Tranche, the members of a Record are represented
    with an Interface.

*/
class Interface
{
public:
    /// The corpus containing the complete metadata.
    Corpus const& corpus;

    /** The aggregated public interfaces.

        This tranche contains all public members of a record
        or namespace.

     */
    std::shared_ptr<Tranche> Public;

    /** The aggregated protected interfaces.

        This tranche contains all protected members of a record
        or namespace.

     */
    std::shared_ptr<Tranche> Protected;

    /** The aggregated private interfaces.

        This tranche contains all private members of a record
        or namespace.

     */
    std::shared_ptr<Tranche> Private;

    /** Creates an Interface object for a given record.

        @param I The record to create the interface for.
        @param corpus The complete metadata.
        @return The interface.
     */
    MRDOCS_DECL
    friend
    Interface
    makeInterface(
        RecordInfo const& I,
        Corpus const& corpus);

private:
    explicit Interface(Corpus const&) noexcept;
};

/** Return the composite interface for a record.

    @return The interface.

    @param I The interface to store the results in.
    @param corpus The complete metadata.
*/
MRDOCS_DECL
Interface
makeInterface(
    RecordInfo const& I,
    Corpus const& corpus);

/** Return the Tranche as a @ref dom::Value object.
 */
MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    std::shared_ptr<Interface> const& sp,
    DomCorpus const* domCorpus);

} // mrdocs::clang

#endif
