//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_METADATA_INTERFACE_HPP
#define MRDOX_METADATA_INTERFACE_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <span>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

enum class Access
{
    None = 0,          // bitstream elides zeroes
    Public,
    Protected,
    Private
};

/** The aggregated interface for a given struct, class, or union.
*/
class Interface
{
public:
    template<class Ty>
    struct Item
    {
        Ty const* I;
        RecordInfo const* From;
        Access access;
    };

    /** A group of children that have the same access specifier.
    */
    struct Tranche
    {
        std::span<Item<EnumInfo> const> Enums;
        std::span<Item<TypedefInfo> const> Types;
        std::span<Item<FunctionInfo> const> Functions;
        std::span<Item<MemberTypeInfo> const> Members;
        std::span<Item<VarInfo> const> Vars;
    };

    /** The aggregated public interfaces.
    */
    Tranche Public;

    /** The aggregated protected interfaces.
    */
    Tranche Protected;

    /** The aggregated private interfaces.
    */
    Tranche Private;

    MRDOX_DECL
    friend
    Interface&
    makeInterface(
        Interface& I,
        RecordInfo const& Derived,
        Corpus const& corpus);

private:
    class Build;

    std::vector<Item<EnumInfo>> enums_;
    std::vector<Item<TypedefInfo>> types_;
    std::vector<Item<FunctionInfo>> functions_;
    std::vector<Item<MemberTypeInfo>> members_;
    std::vector<Item<VarInfo>> vars_;
};

//------------------------------------------------

/** Return the composite interface for a record.

    @return The interface.

    @param I The interface to store the results in.

    @param Derived The most derived record to start from.

    @param corpus The complete metadata.
*/
MRDOX_DECL
Interface&
makeInterface(
    Interface& I,
    RecordInfo const& Derived,
    Corpus const& corpus);

inline
Interface
makeInterface(
    RecordInfo const& Derived,
    Corpus const& corpus)
{
    Interface I;
    return makeInterface(I, Derived, corpus);
}

} // mrdox
} // clang

#endif
