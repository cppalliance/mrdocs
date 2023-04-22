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

#include <mrdox/format/RecursiveWriter.hpp>
#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <mrdox/Reporter.hpp>
#include <llvm/ADT/StringRef.h>
#include <cassert>

namespace clang {
namespace mrdox {

RecursiveWriter::
AllSymbol::
AllSymbol(
    Info const& I)
{
    I.getFullyQualifiedName(fqName);
    symbolType = I.symbolType();
    id = I.id;
}

//------------------------------------------------

RecursiveWriter::
RecursiveWriter(
    llvm::raw_ostream& os,
    Corpus const& corpus,
    Reporter& R) noexcept
    : os_(os)
    , corpus_(corpus)
    , R_(R)
{
}

//------------------------------------------------

void
RecursiveWriter::
visitScope(
    Scope const& I)
{
    for(auto const& ref : I.Namespaces)
        visitNamespace(corpus_.get<NamespaceInfo>(ref.id));
    for(auto const& ref : I.Records)
        visitRecord(corpus_.get<RecordInfo>(ref.id));
    for(auto const& ref : I.Functions)
        visitFunction(corpus_.get<FunctionInfo>(ref.id));
    for(auto const& J : I.Typedefs)
        visitTypedef(J);
    for(auto const& J : I.Enums)
        visitEnum(J);
}

//------------------------------------------------

auto
RecursiveWriter::
makeAllSymbols() ->
    std::vector<AllSymbol>
{
    std::vector<AllSymbol> list;
    list.reserve(corpus_.allSymbols().size());
    for(auto const& id : corpus_.allSymbols())
        list.emplace_back(corpus_.get<Info>(id));
    return list;
}

//------------------------------------------------

llvm::raw_ostream&
RecursiveWriter::
indent()
{
    return os_ << indentString_;
}

void
RecursiveWriter::
adjustNesting(int levels)
{
    if(levels > 0)
    {
        indentString_.append(levels * 2, ' ');
    }
    else
    {
        auto const n = levels * -2;
        assert(n <= indentString_.size());
        indentString_.resize(indentString_.size() - n);
    }
}

} // mrdox
} // clang
