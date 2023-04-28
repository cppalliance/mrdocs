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

#include "Reduce.hpp"
#include <mrdox/Metadata/BaseRecord.hpp>
#include <mrdox/Debug.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <llvm/ADT/STLExtras.h>
#include <cassert>
#include <utility>

namespace clang {
namespace mrdox {

RecordInfo::
RecordInfo(
    SymbolID USR,
    StringRef Name)
    : SymbolInfo(InfoType::IT_record, USR, Name)
    , Children(false)
{
}

} // mrdox
} // clang
