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

#include <mrdox/RecursiveWriter.hpp>
#include <mrdox/Config.hpp>
#include <mrdox/Corpus.hpp>
#include <mrdox/Namespace.hpp>
#include <mrdox/Record.hpp>
#include <mrdox/Reporter.hpp>
#include <mrdox/Scope.hpp>
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
    id = I.USR;
}

//------------------------------------------------

llvm::raw_ostream&
RecursiveWriter::
os() noexcept
{
    if(fd_os_)
        return *fd_os_;
    return *str_os_;
}

llvm::raw_ostream&
RecursiveWriter::
indent()
{
    return os() << indentString_;
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

//------------------------------------------------

RecursiveWriter::
RecursiveWriter(
    Corpus const& corpus,
    Config const& config,
    Reporter& R) noexcept
    : corpus_(corpus)
    , config_(config)
    , R_(R)
{
}

void
RecursiveWriter::
write(llvm::raw_fd_ostream& os)
{
    fd_os_ = &os;
    beginDoc();
    writeAllSymbols(makeAllSymbols());
    visit(corpus_.get<NamespaceInfo>(EmptySID));
    endDoc();
}

void
RecursiveWriter::
write(llvm::raw_string_ostream& os)
{
    str_os_ = &os;
    beginDoc();
    writeAllSymbols(makeAllSymbols());
    visit(corpus_.get<NamespaceInfo>(EmptySID));
    endDoc();
}

//------------------------------------------------

void
RecursiveWriter::
beginDoc()
{
}

void
RecursiveWriter::
endDoc()
{
}

void
RecursiveWriter::
writeAllSymbols(
    std::vector<AllSymbol> const&)
{
}

void
RecursiveWriter::
beginNamespace(
    NamespaceInfo const& I)
{
}

void
RecursiveWriter::
writeNamespace(
    NamespaceInfo const& I)
{
}

void
RecursiveWriter::
endNamespace(
    NamespaceInfo const& I)
{
}

void
RecursiveWriter::
beginRecord(
    RecordInfo const& I)
{
}

void
RecursiveWriter::
writeRecord(
    RecordInfo const& I)
{
}

void
RecursiveWriter::
endRecord(
    RecordInfo const& I)
{
}

void
RecursiveWriter::
beginFunction(
    FunctionInfo const& I)
{
}

void
RecursiveWriter::
writeFunction(
    FunctionInfo const& I)
{
}

void
RecursiveWriter::
endFunction(
    FunctionInfo const& I)
{
}

void
RecursiveWriter::
writeEnum(
    EnumInfo const& I)
{
}
void
RecursiveWriter::
writeTypedef(
    TypedefInfo const& I)
{
}

//------------------------------------------------

void
RecursiveWriter::
visit(
    NamespaceInfo const& I)
{
    beginNamespace(I);
    adjustNesting(1);
    writeNamespace(I);
    visit(I.Children);
    adjustNesting(-1);
    endNamespace(I);
}

void
RecursiveWriter::
visit(
    RecordInfo const& I)
{
    beginRecord(I);
    adjustNesting(1);
    writeRecord(I);
    visit(I.Children);
    adjustNesting(-1);
    endRecord(I);
}

void
RecursiveWriter::
visit(
    Scope const& I)
{
    for(auto const& ref : I.Namespaces)
        visit(corpus_.get<NamespaceInfo>(ref.USR));
    for(auto const& ref : I.Records)
        visit(corpus_.get<RecordInfo>(ref.USR));
    for(auto const& ref : I.Functions)
        visit(corpus_.get<FunctionInfo>(ref.USR));
    for(auto const& J : I.Enums)
        writeEnum(J);
    for(auto const& J : I.Typedefs)
        writeTypedef(J);
}

void
RecursiveWriter::
visit(
    FunctionInfo const& I)
{
    beginFunction(I);
    adjustNesting(1);
    writeFunction(I);
    adjustNesting(-1);
    endFunction(I);
}

auto
RecursiveWriter::
makeAllSymbols() ->
    std::vector<AllSymbol>
{
    std::vector<AllSymbol> list;
    list.reserve(corpus_.allSymbols.size());
    for(auto const& id : corpus_.allSymbols)
        list.emplace_back(corpus_.get<Info>(id));
    return list;
}

} // mrdox
} // clang
