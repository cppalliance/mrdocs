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

//
// This file defines the internal representations of different declaration
// types for the clang-doc tool.
//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_REPRESENTATION_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_DOC_REPRESENTATION_H

#include "jad/AccessScope.hpp"
#include "jad/Info.hpp"
#include "jad/Location.hpp"
#include "jad/Namespace.hpp"
#include "jad/Record.hpp"
#include "jad/Reference.hpp"
#include "jad/ScopeChildren.hpp"
#include "jad/Symbol.hpp"
#include "jad/TemplateParam.hpp"
#include "jad/Type.hpp"
#include "jad/Types.hpp"
#include <clang/AST/Type.h>
#include <clang/Basic/Specifiers.h>
#include <clang/Tooling/StandaloneExecution.h>
#include <llvm/ADT/APSInt.h>
#include <llvm/ADT/SmallVector.h>
#include <llvm/ADT/StringExtras.h>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace clang {
namespace mrdox {

// A standalone function to call to merge a vector of infos into one.
// This assumes that all infos in the vector are of the same type, and will fail
// if they are different.
llvm::Expected<std::unique_ptr<Info>>
mergeInfos(std::vector<std::unique_ptr<Info>>& Values);

} // namespace mrdox
} // namespace clang

#endif
