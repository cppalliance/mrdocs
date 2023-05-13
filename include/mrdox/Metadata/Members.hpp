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

#ifndef MRDOX_METADATA_MEMBERS_HPP
#define MRDOX_METADATA_MEMBERS_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Access.hpp>
#include <mrdox/Metadata/Enum.hpp>
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Metadata/MemberType.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Metadata/Typedef.hpp>
#include <mrdox/Metadata/Var.hpp>

namespace clang {
namespace mrdox {

struct DataMember
{
    MemberTypeInfo const* I;
    RecordInfo const* From;
    Access access;
};

struct MemberEnum
{
    EnumInfo const* I;
    RecordInfo const* From;
    Access access;
};

struct MemberFunction
{
    FunctionInfo const* I;
    RecordInfo const* From;
    Access access;
};

struct MemberRecord
{
    RecordInfo const* I;
    RecordInfo const* From;
    Access access;
};

struct MemberType
{
    TypedefInfo const* I;
    RecordInfo const* From;
    Access access;
};

struct StaticDataMember
{
    VarInfo const* I;
    RecordInfo const* From;
    Access access;
};

} // mrdox
} // clang

#endif
