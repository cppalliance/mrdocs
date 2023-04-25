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
    : AdocWriter(os, fd_os, names_, corpus, R)
    , names_(corpus)
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
    write(I);
    if(! corpus_.visit(I.Children.Namespaces, *this))
        return false;

    // Visit records in alphabetical display order
    std::vector<RecordInfo const*> list;
    list.reserve(I.Children.Records.size());
    for(auto const& ref : I.Children.Records)
        list.push_back(&corpus_.get<RecordInfo>(ref.id));
    std::string s0, s1;
    llvm::sort(list,
        [&s0, &s1](Info const* I0, Info const* I1)
        {
            return compareSymbolNames(
                I0->getFullyQualifiedName(s0),
                I1->getFullyQualifiedName(s1)) < 0;
        });
    for(auto const I : list)
        if(! visit(*I))
            return false;

    return true;
}

bool
AdocSinglePageWriter::
visit(
    RecordInfo const& I)
{
    write(I);
    return true;
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
