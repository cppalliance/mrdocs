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

#ifndef MRDOX_JAD_INDEX_HPP
#define MRDOX_JAD_INDEX_HPP

#include "jad/Info.hpp"
#include "jad/Reference.hpp"
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Error.h>
#include <optional>
#include <vector>

namespace clang {
namespace mrdox {

struct Index : Reference
{
    Index() = default;

    Index(
        llvm::StringRef Name)
        : Reference(SymbolID(), Name)
    {
    }

    Index(
        llvm::StringRef Name,
        llvm::StringRef JumpToSection)
        : Reference(SymbolID(), Name)
        , JumpToSection(JumpToSection)
    {
    }

    Index(
        SymbolID USR,
        llvm::StringRef Name,
        InfoType IT,
        llvm::StringRef Path)
        : Reference(USR, Name, IT, Name, Path)
    {
    }

    // This is used to look for a USR in a vector of Indexes using std::find
    bool operator==(const SymbolID& Other) const
    {
        return USR == Other;
    }

    bool operator<(const Index& Other) const;

    std::optional<llvm::SmallString<16>> JumpToSection;
    std::vector<Index> Children;

    void sort();
};

// A standalone function to call to merge a vector of infos into one.
// This assumes that all infos in the vector are of the same type, and will fail
// if they are different.
llvm::Expected<std::unique_ptr<Info>>
mergeInfos(std::vector<std::unique_ptr<Info>>& Values);

} // mrdox
} // clang

#endif
