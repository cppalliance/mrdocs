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

#include <mrdocs/Platform.hpp>
#include <mrdocs/MetadataFwd.hpp>
#include <mrdocs/Metadata/Record.hpp>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace clang {
namespace mrdocs {

/** A group of children that have the same access specifier.
*/
struct Tranche
{
    #define INFO_PLURAL(Type) std::vector<SymbolID> Type;
    #include <mrdocs/Metadata/InfoNodes.inc>

    std::vector<SymbolID> Types;
    std::vector<SymbolID> StaticFunctions;

    ScopeInfo Overloads;
    ScopeInfo StaticOverloads;
};

/** The aggregated interface for a given struct, class, or union.
*/
class Interface
{
public:
    Corpus const& corpus;

    /** The aggregated public interfaces.
    */
    std::shared_ptr<Tranche> Public;

    /** The aggregated protected interfaces.
    */
    std::shared_ptr<Tranche> Protected;

    /** The aggregated private interfaces.
    */
    std::shared_ptr<Tranche> Private;

    MRDOCS_DECL
    friend
    Interface
    makeInterface(
        RecordInfo const& Derived,
        Corpus const& corpus);

private:
    explicit Interface(Corpus const&) noexcept;
};

//------------------------------------------------

/** Return the composite interface for a record.

    @return The interface.

    @param I The interface to store the results in.

    @param Derived The most derived record to start from.

    @param corpus The complete metadata.
*/
MRDOCS_DECL
Interface
makeInterface(
    RecordInfo const& Derived,
    Corpus const& corpus);

/** Return a tranche representing the members of a namespace.

    @return The tranche.

    @param Derived The namespace to build the tranche for.

    @param corpus The complete metadata.
*/
MRDOCS_DECL
Tranche
makeTranche(
    NamespaceInfo const& Namespace,
    Corpus const& corpus);

} // mrdocs
} // clang

#endif
