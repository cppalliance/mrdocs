//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_SYMBOL_RECORDINTERFACE_HPP
#define MRDOCS_API_METADATA_SYMBOL_RECORDINTERFACE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbol/RecordTranche.hpp>

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

MRDOCS_DECL
void
merge(RecordInterface& I, RecordInterface&& Other);

/** Map a RecordInterface to a dom::Object.

    @param io The output parameter to receive the dom::Object.
    @param I The RecordInterface to convert.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    RecordInterface const& I,
    DomCorpus const*)
{
    io.map("public", I.Public);
    io.map("protected", I.Protected);
    io.map("private", I.Private);
}

/** Map the RecordInterface to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    RecordInterface const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}


inline
auto
allMembers(RecordInterface const& T)
{
    // This is a trick to emulate views::concat in C++20
    return
        std::views::iota(0, 3) |
        std::views::transform(
            [&T](int i) -> auto {
                switch (i) {
                    case 0: return allMembers(T.Public);
                    case 1: return allMembers(T.Protected);
                    case 2: return allMembers(T.Private);
                    default: throw std::out_of_range("Invalid index");
                }
            }) |
        std::ranges::views::join;
}

} // mrdocs

#endif // MRDOCS_API_METADATA_SYMBOL_RECORDINTERFACE_HPP
