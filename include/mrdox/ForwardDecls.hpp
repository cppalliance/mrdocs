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

#ifndef MRDOX_FORWARD_DECLS_HPP
#define MRDOX_FORWARD_DECLS_HPP

namespace clang {
namespace mrdox {

class Config;
class Corpus;

struct BaseRecordInfo;
struct EnumInfo;
struct FieldTypeInfo;
struct FunctionInfo;
struct Info;
struct Location;
struct NamespaceInfo;
struct RecordInfo;
struct Scope;
struct SymbolInfo;
struct TemplateParamInfo;
struct TypedefInfo;

struct Reporter;

} // mrdox
} // clang

#endif
