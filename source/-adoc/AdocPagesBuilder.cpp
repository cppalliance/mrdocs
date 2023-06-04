//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "AdocPagesBuilder.hpp"
#include "AdocMultiPageWriter.hpp"
#include "Support/Error.hpp"
#include "Support/Radix.hpp"
#include <mrdox/Metadata.hpp>
#include <mrdox/Support/Report.hpp>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {
namespace adoc {

Error
AdocPagesBuilder::
build()
{
    corpus_.traverse(*this, SymbolID::zero);
    wg_.wait();
    return {};
}

template<class T>
void
AdocPagesBuilder::
build(
    T const& I)
{
    namespace fs = llvm::sys::fs;
    namespace path = llvm::sys::path;

    wg_.post(
        [&]
        {
            llvm::SmallString<512> filePath = outputPath_;
            llvm::StringRef name = names_.get(I.id);
            path::append(filePath, name);
            filePath.append(".adoc");
            std::error_code ec;
            llvm::raw_fd_ostream os(filePath, ec, fs::CD_CreateAlways);
            if(ec)
            {
                reportError(
                    Error("raw_fd_ostream(\"{}\") returned \"{}\"", filePath, ec.message()),
                    "generate Asciidoc reference");
                return;
            }
            AdocMultiPageWriter writer(os, corpus_, names_);
            writer.build(I);
            if(auto ec = os.error())
            {
                reportError(
                    Error("AdocMultiPageWriter returned \"{}\"", ec.message()),
                    "generate Asciidoc reference");
                return;
            }
        });
}

bool
AdocPagesBuilder::
visit(
    NamespaceInfo const& I)
{
    corpus_.traverse(*this, I);
    return true;
}

bool
AdocPagesBuilder::
visit(
    RecordInfo const& I)
{
    build(I);
    corpus_.traverse(*this, I);
    return true;
}

bool
AdocPagesBuilder::
visit(
    FunctionInfo const& I)
{
    build(I);
    return true;
}

bool
AdocPagesBuilder::
visit(
    TypedefInfo const& I)
{
    build(I);
    return true;
}

bool
AdocPagesBuilder::
visit(
    EnumInfo const& I)
{
    build(I);
    return true;
}

} // adoc
} // mrdox
} // clang
