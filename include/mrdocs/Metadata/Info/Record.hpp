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

#ifndef MRDOCS_API_METADATA_RECORD_HPP
#define MRDOCS_API_METADATA_RECORD_HPP

#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/Source.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <vector>
#include <ranges>

namespace clang::mrdocs {

/** A group of members that have the same access specifier.

    This struct represents a collection of symbols that share
    the same access specifier within a record.

    It includes one vector for each info type allowed in a
    record, and individual vectors for static functions, types,
    and function overloads.
*/
struct RecordTranche
{
    std::vector<SymbolID> NamespaceAliases;
    std::vector<SymbolID> Typedefs;
    std::vector<SymbolID> Records;
    std::vector<SymbolID> Enums;
    std::vector<SymbolID> Functions;
    std::vector<SymbolID> StaticFunctions;
    std::vector<SymbolID> Variables;
    std::vector<SymbolID> StaticVariables;
    std::vector<SymbolID> Concepts;
    std::vector<SymbolID> Guides;
    std::vector<SymbolID> Friends;
    std::vector<SymbolID> Usings;
};

MRDOCS_DECL
void
merge(RecordTranche& I, RecordTranche&& Other);

inline
auto
allMembers(RecordTranche const& T)
{
    // This is a trick to emulate views::concat in C++20
    return std::views::transform(
        std::views::iota(0, 12),
        [&T](int const i) -> auto const&
        {
            switch (i) {
                case 0: return T.NamespaceAliases;
                case 1: return T.Typedefs;
                case 2: return T.Records;
                case 3: return T.Enums;
                case 4: return T.Functions;
                case 5: return T.StaticFunctions;
                case 6: return T.Variables;
                case 7: return T.StaticVariables;
                case 8: return T.Concepts;
                case 9: return T.Guides;
                case 10: return T.Friends;
                case 11: return T.Usings;
                default: throw std::out_of_range("Invalid index");
            }
        }
    ) | std::ranges::views::join;
}

/** Map a RecordTranche to a dom::Object.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    RecordTranche const& I,
    DomCorpus const* domCorpus)
{
    io.map("namespaceAliases", dom::LazyArray(I.NamespaceAliases, domCorpus));
    io.map("typedefs", dom::LazyArray(I.Typedefs, domCorpus));
    io.map("records", dom::LazyArray(I.Records, domCorpus));
    io.map("enums", dom::LazyArray(I.Enums, domCorpus));
    io.map("functions", dom::LazyArray(I.Functions, domCorpus));
    io.map("staticFunctions", dom::LazyArray(I.StaticFunctions, domCorpus));
    io.map("variables", dom::LazyArray(I.Variables, domCorpus));
    io.map("staticVariables", dom::LazyArray(I.StaticVariables, domCorpus));
    io.map("concepts", dom::LazyArray(I.Concepts, domCorpus));
    io.map("guides", dom::LazyArray(I.Guides, domCorpus));
    io.map("friends", dom::LazyArray(I.Friends, domCorpus));
    io.map("usings", dom::LazyArray(I.Usings, domCorpus));
}

/** Map the RecordTranche to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    RecordTranche const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

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

/** Metadata for a direct base.
*/
struct BaseInfo
{
    Polymorphic<TypeInfo> Type;
    AccessKind Access = AccessKind::Public;
    bool IsVirtual = false;

    BaseInfo() = default;

    BaseInfo(
        Polymorphic<TypeInfo>&& type,
        AccessKind const access,
        bool const is_virtual)
        : Type(std::move(type))
        , Access(access)
        , IsVirtual(is_virtual)
    {
    }
};

MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    BaseInfo const& I,
    DomCorpus const* domCorpus);

enum class RecordKeyKind
{
    Struct,
    Class,
    Union
};

MRDOCS_DECL
dom::String
toString(RecordKeyKind kind) noexcept;

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    RecordKeyKind kind)
{
    v = toString(kind);
}

/** Metadata for struct, class, or union.
*/
struct RecordInfo final
    : InfoCommonBase<InfoKind::Record>
{
    /** Kind of record this is (class, struct, or union).
    */
    RecordKeyKind KeyKind = RecordKeyKind::Struct;

    /// When present, this record is a template or specialization.
    std::optional<TemplateInfo> Template;

    // Indicates if the record was declared using a typedef.
    // Things like anonymous structs in a typedef:
    //   typedef struct { ... } foo_t;
    // are converted into records with the typedef as the Name + this flag set.
    // KRYSTIAN FIXME: this does not account for alias-declarations
    bool IsTypeDef = false;

    bool IsFinal = false;
    bool IsFinalDestructor = false;

    /** List of immediate bases.
    */
    std::vector<BaseInfo> Bases;

    RecordInterface Interface;

    //--------------------------------------------

    explicit RecordInfo(SymbolID const& ID) noexcept
        : InfoCommonBase(ID)
    {
    }

    std::strong_ordering
    operator<=>(const RecordInfo& other) const;
};

constexpr
std::string_view
getDefaultAccessString(
    RecordKeyKind const& kind) noexcept
{
    switch(kind)
    {
    case RecordKeyKind::Class:
        return "private";
    case RecordKeyKind::Struct:
    case RecordKeyKind::Union:
        return "public";
    default:
        MRDOCS_UNREACHABLE();
    }
}

inline
auto
allMembers(RecordInfo const& T)
{
    return allMembers(T.Interface);
}

MRDOCS_DECL
void
merge(RecordInfo& I, RecordInfo&& Other);

/** Map a RecordInfo to a dom::Object.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    RecordInfo const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Info const&>(I), domCorpus);
    io.map("tag", I.KeyKind);
    io.map("defaultAccess", getDefaultAccessString(I.KeyKind));
    io.map("isTypedef", I.IsTypeDef);
    io.map("bases", dom::LazyArray(I.Bases, domCorpus));
    io.map("interface", I.Interface);
    io.map("template", I.Template);
}

/** Map the RecordInfo to a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    RecordInfo const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif
