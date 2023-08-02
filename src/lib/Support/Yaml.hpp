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

#ifndef MRDOX_LIB_SUPPORT_YAML_HPP
#define MRDOX_LIB_SUPPORT_YAML_HPP

#include <mrdox/Platform.hpp>
#include <llvm/Support/YAMLTraits.h>

namespace clang {
namespace mrdox {

class MRDOX_DECL
    YamlReporter
{
    void diag(llvm::SMDiagnostic const&);
    static void diagFnImpl(llvm::SMDiagnostic const&, void*);

public:
    typedef void (*diagFn)(llvm::SMDiagnostic const&, void*);

    operator diagFn() const noexcept
    {
        return &diagFnImpl;
    }
};

} // mrdox
} // clang

#endif
