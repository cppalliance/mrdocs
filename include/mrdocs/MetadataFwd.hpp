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

#ifndef MRDOCS_API_METADATAFWD_HPP
#define MRDOCS_API_METADATAFWD_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbols.hpp>

// Forward declarations for all types
// related to metadata extracted from AST

namespace clang {
namespace mrdocs {

class Corpus;

class Interface;

enum class InfoKind;

enum class AccessKind;
enum class ConstexprKind;
enum class ExplicitKind;
enum class NoexceptKind;
enum class OperatorKind;
enum class ReferenceKind;
enum class StorageClassKind;

struct BaseInfo;
struct EnumInfo;
struct EnumeratorInfo;
struct FieldInfo;
struct FriendInfo;
struct FunctionInfo;
struct Info;
class Javadoc;
struct Location;
struct NamespaceInfo;
struct RecordInfo;
struct Param;
struct SpecializationInfo;
struct SpecializedMember;
struct SourceInfo;
struct TypeInfo;
struct TypedefInfo;
struct VariableInfo;
struct VerbatimBlock;

struct ExprInfo;
template<typename T>
struct ConstantExprInfo;

struct TemplateInfo;
struct TArg;
struct TypeTArg;
struct NonTypeTArg;
struct TemplateTArg;
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

} // mrdocs
} // clang

#endif
