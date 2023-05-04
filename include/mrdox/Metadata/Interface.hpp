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
#include <mrdox/Metadata/Access.hpp>
#include <span>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

struct RecordWithAccess
{
    RecordInfo const* I;
    RecordInfo const* From;
    Access access;
};

struct FunctionWithAccess
{
    FunctionInfo const* I;
    RecordInfo const* From;
    Access access;
};

struct EnumWithAccess
{
    EnumInfo const* I;
    RecordInfo const* From;
    Access access;
};

struct TypeWithAccess
{
    TypedefInfo const* I;
    RecordInfo const* From;
    Access access;
};

struct VarWithAccess
{
    VarInfo const* I;
    RecordInfo const* From;
    Access access;
};

struct DataWithAccess
{
    MemberTypeInfo const* I;
    RecordInfo const* From;
    Access access;
};

/** The aggregated interface for a given struct, class, or union.
*/
class Interface
{
public:
    /** A group of children that have the same access specifier.
    */
    struct Tranche
    {
        std::span<RecordWithAccess const> Records;
        std::span<FunctionWithAccess const> Functions;
        std::span<EnumWithAccess const> Enums;
        std::span<TypeWithAccess const> Types;
        std::span<DataWithAccess const> Data;
        std::span<VarWithAccess const> Vars;
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

    std::vector<RecordWithAccess> records_;
    std::vector<FunctionWithAccess> functions_;
    std::vector<EnumWithAccess> enums_;
    std::vector<TypeWithAccess> types_;
    std::vector<DataWithAccess> data_;
    std::vector<VarWithAccess> vars_;
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
