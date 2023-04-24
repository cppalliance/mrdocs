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

#ifndef MRDOX_METADATA_BASERECORD_HPP
#define MRDOX_METADATA_BASERECORD_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Record.hpp>

namespace clang {
namespace mrdox {

struct BaseRecordInfo : public RecordInfo
{
    BaseRecordInfo();

    BaseRecordInfo(
        SymbolID id_,
        llvm::StringRef Name,
        bool IsVirtual,
        AccessSpecifier Access,
        bool IsParent);

    // Indicates if base corresponds to a virtual inheritance
    bool IsVirtual = false;

    // Access level associated with this inherited info (public, protected,
    // private).
    AccessSpecifier Access = AccessSpecifier::AS_public;

    bool IsParent = false; // Indicates if this base is a direct parent
};

} // mrdox
} // clang

#endif
