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
#include "Support/Radix.hpp"
#include <mrdox/Metadata.hpp>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdox {
namespace adoc {

llvm::Error
AdocPagesBuilder::
build()
{
    corpus_.visit(globalNamespaceID, *this);
    wg_.wait();
    return llvm::Error::success();
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
            if(! R_.error(ec, "open '", filePath, "'"))
            {
                AdocMultiPageWriter writer(os, &os, corpus_, names_, R_);
                writer.build(I);
                if(auto ec = os.error())
                    if(R_.error(ec, "write '", filePath, "'"))
                        return;
            }
        });
}

bool
AdocPagesBuilder::
visit(
    NamespaceInfo const& I)
{
    corpus_.visit(I.Children, *this);
    return true;
}

bool
AdocPagesBuilder::
visit(
    RecordInfo const& I)
{
    build(I);
    corpus_.visit(I.Children, *this);
    return true;
}

bool
AdocPagesBuilder::
visit(
    Overloads const& I)
{
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
