//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "PagesBuilder.hpp"
#include <mrdox/Metadata.hpp>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {

/*
    Pages are as follows:

    Class
    Class Template
    Class Template Specialization 
    OverloadSet
    Nested Class
    Free Function
    Variable/Constant
    Typedef
    Enum

    Page name:

    /{namespace}/{symbol}.html
*/

void
PagesBuilder::
scan()
{
    // visit the children not the namespace
    corpus_.visitWithOverloads(corpus_.globalNamespace().Children, *this);

    std::sort(pages.begin(), pages.end(),
        [](Page& p0, Page& p1)
        {
            return Corpus::symbolCompare(
                p0.fileName, p1.fileName);
        });
}

void
PagesBuilder::
visit(
    NamespaceInfo const& I)
{
    namespace path = llvm::sys::path;

    auto saved = filePrefix_;
    path::append(filePrefix_, I.Name);
    corpus_.visit(I.Children, *this);
    filePrefix_ = saved;
}

void
PagesBuilder::
visit(
    RecordInfo const& I)
{
    namespace path = llvm::sys::path;

    int i = 0;
    if(I.Name == "ipv4_address")
    {
        i = 1;
    }

    auto filePath = filePrefix_;
    path::append(filePath, I.Name);
    path::replace_extension(filePath, "adoc");
    pages.emplace_back(std::move(filePath));

    auto saved = filePrefix_;
    path::append(filePrefix_, I.Name);
    corpus_.visitWithOverloads(I.Children, *this);
    filePrefix_ = saved;

}

void
PagesBuilder::
visit(
    FunctionOverloads const& I)
{
    namespace path = llvm::sys::path;

    auto filePath = filePrefix_;
    path::append(filePath, I.name);
    path::replace_extension(filePath, "adoc");
    pages.emplace_back(std::move(filePath));
}

void
PagesBuilder::
visit(
    FunctionInfo const& I)
{
}

void
PagesBuilder::
visit(
    TypedefInfo const& I)
{
    namespace path = llvm::sys::path;

    auto filePath = filePrefix_;
    path::append(filePath, I.Name);
    path::replace_extension(filePath, "adoc");
    pages.emplace_back(std::move(filePath));
}

void
PagesBuilder::
visit(
    EnumInfo const& I)
{
    namespace path = llvm::sys::path;

    auto filePath = filePrefix_;
    path::append(filePath, I.Name);
    path::replace_extension(filePath, "adoc");
    pages.emplace_back(std::move(filePath));
}

} // mrdox
} // clang
