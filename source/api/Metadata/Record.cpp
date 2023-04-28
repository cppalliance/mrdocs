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

#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Metadata/BaseRecord.hpp>

namespace clang {
namespace mrdox {

// This is here base BaseRecordInfo inherits from RecordInfo
RecordInfo::
RecordInfo(
    SymbolID id,
    llvm::StringRef Name)
    : SymbolInfo(InfoType::IT_record, id, Name)
    , Children(false)
{
}

} // mrdox
} // clang
