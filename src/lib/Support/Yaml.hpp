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

#ifndef MRDOCS_LIB_SUPPORT_YAML_HPP
#define MRDOCS_LIB_SUPPORT_YAML_HPP

#include <mrdocs/Platform.hpp>
#include <llvm/Support/YAMLTraits.h>

namespace clang {
namespace mrdocs {

class MRDOCS_DECL
    YamlReporter
{
  static void diag(llvm::SMDiagnostic const &, void *);

public:
    typedef void (*diagFn)(llvm::SMDiagnostic const&, void*);

    operator diagFn() const noexcept { return &diag; }
};

} // mrdocs
} // clang

#endif
