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
    endSection();
    return llvm::Error::success();
}

//------------------------------------------------

template<class Type>
std::vector<Type const*>
AdocSinglePageWriter::
buildSortedList(
    std::vector<Reference> const& from) const
{
    std::vector<Type const*> result;
    result.reserve(from.size());
    for(auto const& ref : from)
        result.push_back(&corpus_.get<Type>(ref.id));
    llvm::sort(result,
        [&](Info const* I0, Info const* I1)
        {
            return compareSymbolNames(
                I0->Name, I1->Name) < 0;
        });
    return result;
}

/*  Write a namespace.

    This will index all individual
    symbols except child namespaces,
    sorted by group.
*/
bool
AdocSinglePageWriter::
visit(
    NamespaceInfo const& I)
{
    //if(! corpus_.visit(I.Children.Namespaces, *this))
        //return false;
/*
    if( I.Children.Records.empty() &&
        I.Children.Functions.empty() &&
        I.Children.Typedefs.empty() &&
        I.Children.Enums.empty())
        return;
*/
    // build sorted list of namespaces,
    // this is for visitation not display.
    auto namespaceList = buildSortedList<NamespaceInfo>(I.Children.Namespaces);

    // don't emit empty namespaces,
    // but still visit child namespaces.
    if( ! I.Children.Records.empty() ||
        ! I.Children.Functions.empty() ||
        ! I.Children.Typedefs.empty() ||
        ! I.Children.Enums.empty())
    {
        std::string s;
        I.getFullyQualifiedName(s);
        s = "namespace " + s;

        beginSection(s);

        auto recordList = buildSortedList<RecordInfo>(I.Children.Records);
        auto functionList = buildSortedList<FunctionInfo>(I.Children.Functions);
        //auto typeList = ?
        //auto enumList = ?

        if(! recordList.empty())
        {
            beginSection("Classes");
            os_ << "\n"
                "[cols=1]\n"
                "|===\n";
            for(auto const I : recordList)
            {
                os_ << "\n|" << linkFor(*I) << '\n';
            };
            os_ << "|===\n";
            endSection();
        }

        if(! functionList.empty())
        {
            beginSection("Functions");
            os_ << "\n"
                "[cols=1]\n"
                "|===\n";
            for(auto const I : functionList)
            {
                os_ << "\n|" << linkFor(*I) << '\n';
            };
            os_ << "|===\n";
            endSection();
        }

        //if(! typeList.empty())

        //if(! enumList.empty())

        // now visit each indexed item
        for(auto const& I : recordList)
            visit(*I);
        recordList.clear();

        for(auto const& I : functionList)
            visit(*I);
        functionList.clear();

        /*
        for(auto const& I : typeList)
            visit(*I);
        typeList.clear();
        */

        /*
        for(auto const& I : enumList)
            visit(*I);
        typeList.clear();
        */

        endSection();
    }

    // visit child namespaces
    for(auto const& I : namespaceList)
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
