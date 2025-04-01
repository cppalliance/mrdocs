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

// Forward declarations for all types
// related to metadata extracted from AST

namespace clang::mrdocs {

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

#define INFO(Type) \
struct Type##Info;
#include <mrdocs/Metadata/Info/InfoNodes.inc>

struct BaseInfo;
struct Info;
struct Javadoc;
struct Location;
struct NameInfo;
struct Param;
struct SpecializationNameInfo;
struct SourceInfo;
struct TypeInfo;
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

} // clang::mrdocs

#endif
