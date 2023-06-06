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

#ifndef MRDOX_API_METADATAFWD_HPP
#define MRDOX_API_METADATAFWD_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Metadata/Symbols.hpp>

// Forward declarations for all types
// related to metadata extracted from AST

namespace clang {
namespace mrdox {

class Corpus;

class Interface;

enum class Access;
enum class FunctionKind;

struct BaseInfo;
struct EnumValueInfo;
struct EnumInfo;
struct FieldInfo;
struct FunctionInfo;
struct Info;
struct Javadoc;
struct Location;
struct MemberRef;
struct NamespaceInfo;
struct RecordInfo;
struct RecordScope;
struct Param;
struct Reference;
struct Scope;
struct SpecializationInfo;
struct SpecializedMember;
struct SymbolInfo;
struct TypeInfo;
struct TypedefInfo;
struct VarInfo;
struct VerbatimBlock;

struct TemplateInfo;
struct TArg;
struct TParam;
struct TypeTParam;
struct NonTypeTParam;
struct TemplateTParam;

struct DataMember;
struct MemberEnum;
struct MemberFunction;
struct MemberRecord;
struct MemberType;
struct StaticDataMember;

template<unsigned char Offset,
         unsigned char Size,
         typename T>
struct BitField;
using BitFieldFullValue = BitField<0, 32, std::uint32_t>;

} // mrdox
} // clang

#endif
