//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "lib/Support/Error.hpp"
#include "lib/Support/Yaml.hpp"

namespace clang {
namespace mrdocs {

void
YamlReporter::
diag(
    llvm::SMDiagnostic const& D)
{
    report::Level level = report::Level::debug;
    switch(D.getKind())
    {
    case llvm::SourceMgr::DiagKind::DK_Remark:
        level = report::Level::debug;
        break;
    case llvm::SourceMgr::DiagKind::DK_Note:
        level = report::Level::info;
        break;
    case llvm::SourceMgr::DiagKind::DK_Warning:
    {
        if(D.getMessage().starts_with("unknown key "))
        {
            // don't show these
            return;
        }
        level = report::Level::warn;
        break;
    }
    case llvm::SourceMgr::DiagKind::DK_Error:
        level = report::Level::error;
        break;
    default:
        MRDOCS_UNREACHABLE();
    }

    report::call_impl(
        level,
        [&](llvm::raw_ostream& os)
        {
            D.print("mrdocs", os, true, true);
        }, nullptr);
}

void
YamlReporter::
diagFnImpl(
    llvm::SMDiagnostic const& D, void* ctx)
{
    reinterpret_cast<YamlReporter*>(ctx)->diag(D);
}

} // mrdocs
} // clang
