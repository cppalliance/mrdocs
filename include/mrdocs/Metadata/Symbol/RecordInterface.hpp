//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_RECORDINTERFACE_HPP
#define MRDOCS_API_METADATA_SYMBOL_RECORDINTERFACE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbol/RecordTranche.hpp>
#include <mrdocs/Support/Reflection/Describe.hpp>

namespace mrdocs {

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
class RecordInterface
{
public:
    /** The aggregated public interfaces.

        This tranche contains all public members of a record
        or namespace.

    */
    RecordTranche Public;

    /** The aggregated protected interfaces.

        This tranche contains all protected members of a record
        or namespace.

    */
    RecordTranche Protected;

    /** The aggregated private interfaces.

        This tranche contains all private members of a record
        or namespace.

    */
    RecordTranche Private;
};

MRDOCS_DESCRIBE_STRUCT(
    RecordInterface,
    (),
    (Public, Protected, Private)
)

/** Flatten all public/protected/private members.
    @return View concatenating the three access tranches.
*/
inline
auto
allMembers(RecordInterface const& T)
{
    // Concatenate the access tranches (emulating C++26 views::concat).
    // The tranches are discovered by reflection, so this needs no
    // hand-maintained per-tranche switch.
    static constexpr auto tranches =
        describe::memberPointers<RecordInterface>();
    return tranches
        | std::views::transform(
            [&T](auto const p) { return allMembers(T.*p); })
        | std::ranges::views::join;
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_RECORDINTERFACE_HPP
