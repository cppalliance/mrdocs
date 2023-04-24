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

#include "AdocSinglePageWriter.hpp"

namespace clang {
namespace mrdox {
namespace adoc {

AdocSinglePageWriter::
AdocSinglePageWriter(
    llvm::raw_ostream& os,
    llvm::raw_fd_ostream* fd_os,
    Corpus const& corpus,
    Reporter& R) noexcept
    : AdocWriter(os, fd_os, corpus, R)
{
}

llvm::Error
AdocSinglePageWriter::
build()
{
    Assert(sect_.level == 0);
    sect_.level = 1;
    sect_.markup = "=";
    os_ <<
        "= Reference\n"
        ":role: mrdox\n";
    corpus_.visit(globalNamespaceID, *this);
    closeSection();
    return llvm::Error::success();
}

bool
AdocSinglePageWriter::
visit(
    NamespaceInfo const& I)
{
    return corpus_.visit(I.Children, *this);
}

bool
AdocSinglePageWriter::
visit(
    RecordInfo const& I)
{
    write(I);
    return corpus_.visit(I.Children, *this);
}

bool
AdocSinglePageWriter::
visit(
    FunctionInfo const& I)
{
    write(I);
    return true;
}

bool
AdocSinglePageWriter::
visit(
    TypedefInfo const& I)
{
    write(I);
    return true;
}

bool
AdocSinglePageWriter::
visit(
    EnumInfo const& I)
{
    write(I);
    return true;
}

} // adoc
} // mrdox
} // clang
