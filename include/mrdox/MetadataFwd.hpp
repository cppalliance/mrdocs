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

#ifndef MRDOX_METADATA_FWD_HPP
#define MRDOX_METADATA_FWD_HPP

// Forward declarations for all types
// related to metadata extracted from AST

#include <mrdox/meta/Symbols.hpp>

namespace clang {
namespace mrdox {

struct BaseRecordInfo;
struct EnumValueInfo;
struct EnumInfo;
struct FieldTypeInfo;
struct FunctionInfo;
struct Overloads;
struct OverloadsSet;
struct Info;
struct Javadoc;
struct Location;
struct MemberTypeInfo;
struct NamespaceInfo;
struct RecordInfo;
struct Reference;
struct Scope;
struct SymbolInfo;
struct TemplateInfo;
struct TemplateParamInfo;
struct TemplateSpecializationInfo;
struct TypeInfo;
struct TypedefInfo;
struct VerbatimBlock;

} // mrdox
} // clang

#endif
