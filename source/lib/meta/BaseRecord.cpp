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

#include <mrdox/meta/BaseRecord.hpp>

namespace clang {
namespace mrdox {

BaseRecordInfo::
BaseRecordInfo()
    : RecordInfo()
{
}

BaseRecordInfo::
BaseRecordInfo(
    SymbolID USR,
    StringRef Name,
    bool IsVirtual,
    AccessSpecifier Access,
    bool IsParent)
    : RecordInfo(USR, Name)
    , IsVirtual(IsVirtual)
    , Access(Access)
    , IsParent(IsParent)
{
}

} // mrdox
} // clang
