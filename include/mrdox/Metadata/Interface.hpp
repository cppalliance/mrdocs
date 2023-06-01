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
#include <mrdox/Metadata/Members.hpp>
#include <span>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

/** The aggregated interface for a given struct, class, or union.
*/
class Interface
{
public:
    /** A group of children that have the same access specifier.
    */
    struct Tranche
    {
        std::span<MemberRecord const> Records;
        std::span<MemberFunction const> Functions;
        std::span<MemberEnum const> Enums;
        std::span<MemberType const> Types;
        std::span<DataMember const> Data;
        std::span<StaticDataMember const> Vars;
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

    std::vector<DataMember> data_;
    std::vector<MemberEnum> enums_;
    std::vector<MemberFunction> functions_;
    std::vector<MemberRecord> records_;
    std::vector<MemberType> types_;
    std::vector<StaticDataMember> vars_;
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
    makeInterface(I, Derived, corpus);
    return I;
}

} // mrdox
} // clang

#endif
