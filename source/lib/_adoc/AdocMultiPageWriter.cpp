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

#include "AdocMultiPageWriter.hpp"

namespace clang {
namespace mrdox {
namespace adoc {

AdocMultiPageWriter::
AdocMultiPageWriter(
    llvm::raw_ostream& os,
    llvm::raw_fd_ostream* fd_os,
    Corpus const& corpus,
    SafeNames const& names,
    Reporter& R) noexcept
    : AdocWriter(os, fd_os, corpus, R)
    , names_(names)
{
}

void
AdocMultiPageWriter::
build(
    RecordInfo const& I)
{
    writeTitle(I);
    os_ << '\n';
    write(I);
}

void
AdocMultiPageWriter::
build(
    FunctionInfo const& I)
{
    writeTitle(I);
    os_ << '\n';
    write(I);
}

void
AdocMultiPageWriter::
build(
    TypedefInfo const& I)
{
    writeTitle(I);
    os_ << '\n';
    write(I);
}

void
AdocMultiPageWriter::
build(
    EnumInfo const& I)
{
    writeTitle(I);
    os_ << '\n';
    write(I);
}

void
AdocMultiPageWriter::
writeTitle(Info const& I)
{
    Assert(sect_.level == 0);
    sect_.level = 1;
    sect_.markup = "=";
    os_ <<
        "= " << I.Name << "\n"
        ":role: mrdox\n";
}

} // adoc
} // mrdox
} // clang