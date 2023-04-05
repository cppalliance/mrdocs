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

#include "Functions.h"
#include "jad/AccessScope.hpp"
#include "jad/Info.hpp"
#include "jad/Location.hpp"
#include "jad/Namespace.hpp"
#include "jad/Reference.hpp"
#include "jad/ScopeChildren.hpp"
#include "jad/Symbol.hpp"
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

struct BaseRecordInfo;
struct EnumInfo;
struct FunctionInfo;
struct TypedefInfo;

// A base struct for TypeInfos
struct TypeInfo
{
    TypeInfo() = default;
    TypeInfo(const Reference& R) : Type(R) {}

    // Convenience constructor for when there is no symbol ID or info type
    // (normally used for built-in types in tests).
    TypeInfo(StringRef Name, StringRef Path = StringRef())
        : Type(SymbolID(), Name, InfoType::IT_default, Name, Path) {}

    bool operator==(const TypeInfo& Other) const { return Type == Other.Type; }

    Reference Type; // Referenced type in this info.
};

// Represents one template parameter.
//
// This is a very simple serialization of the text of the source code of the
// template parameter. It is saved in a struct so there is a place to add the
// name and default values in the future if needed.
//
/** A template parameter.
*/
struct TemplateParamInfo
{
    TemplateParamInfo() = default;

    explicit
    TemplateParamInfo(
        NamedDecl const& ND);

    TemplateParamInfo(
        Decl const& D,
        TemplateArgument const &Arg);

    TemplateParamInfo(
        StringRef Contents)
        : Contents(Contents)
    {
    }

    // The literal contents of the code for that specifies this template parameter
    // for this declaration. Typical values will be "class T" and
    // "typename T = int".
    SmallString<16> Contents;
};

struct TemplateSpecializationInfo {
    // Indicates the declaration that this specializes.
    SymbolID SpecializationOf;

    // Template parameters applying to the specialized record/function.
    std::vector<TemplateParamInfo> Params;
};

// Records the template information for a struct or function that is a template
// or an explicit template specialization.
struct TemplateInfo {
    // May be empty for non-partial specializations.
    std::vector<TemplateParamInfo> Params;

    // Set when this is a specialization of another record/function.
    std::optional<TemplateSpecializationInfo> Specialization;
};

// Info for field types.
struct FieldTypeInfo
    : public TypeInfo
{
    FieldTypeInfo() = default;

    FieldTypeInfo(
        TypeInfo const& TI,
        StringRef Name = StringRef(),
        StringRef DefaultValue = StringRef())
        : TypeInfo(TI)
        , Name(Name)
        , DefaultValue(DefaultValue)
    {
    }

    bool operator==(const FieldTypeInfo& Other) const {
        return std::tie(Type, Name, DefaultValue) ==
            std::tie(Other.Type, Other.Name, Other.DefaultValue);
    }

    SmallString<16> Name; // Name associated with this info.

    // When used for function parameters, contains the string representing the
    // expression of the default value, if any.
    SmallString<16> DefaultValue;
};

// Info for member types.
struct MemberTypeInfo : public FieldTypeInfo
{
    MemberTypeInfo() = default;
    MemberTypeInfo(const TypeInfo& TI, StringRef Name, AccessSpecifier Access)
        : FieldTypeInfo(TI, Name), Access(Access) {}

    bool operator==(const MemberTypeInfo& Other) const {
        return std::tie(Type, Name, Access, Description) ==
            std::tie(Other.Type, Other.Name, Other.Access, Other.Description);
    }

    // VFALCO Why public?
    // Access level associated with this info (public, protected, private, none).
    // AS_public is set as default because the bitcode writer requires the enum
    // with value 0 to be used as the default.
    // (AS_public = 0, AS_protected = 1, AS_private = 2, AS_none = 3)
    AccessSpecifier Access = AccessSpecifier::AS_public;

    Javadoc javadoc;
    std::vector<CommentInfo> Description; // Comment description of this field.
};

//------------------------------------------------

// TODO: Expand to allow for documenting templating and default args.
// Info for functions.
struct FunctionInfo : public SymbolInfo {
    FunctionInfo(SymbolID USR = SymbolID())
        : SymbolInfo(InfoType::IT_function, USR) {}

    void merge(FunctionInfo&& I);

    bool IsMethod = false; // Indicates whether this function is a class method.
    Reference Parent;      // Reference to the parent class decl for this method.
    TypeInfo ReturnType;   // Info about the return type of this function.
    llvm::SmallVector<FieldTypeInfo, 4> Params; // List of parameters.
    // Access level for this method (public, private, protected, none).
    // AS_public is set as default because the bitcode writer requires the enum
    // with value 0 to be used as the default.
    // (AS_public = 0, AS_protected = 1, AS_private = 2, AS_none = 3)
    AccessSpecifier Access = AccessSpecifier::AS_public;

    // Full qualified name of this function, including namespaces and template
    // specializations.
    SmallString<16> FullName;

    // When present, this function is a template or specialization.
    std::optional<TemplateInfo> Template;
};

// TODO: Expand to allow for documenting templating, inheritance access,
// friend classes
// Info for types.
struct RecordInfo
    : public SymbolInfo
{
    RecordInfo(SymbolID USR = SymbolID(), StringRef Name = StringRef(),
        StringRef Path = StringRef());

    void merge(RecordInfo&& I);

    // Type of this record (struct, class, union, interface).
    TagTypeKind TagType = TagTypeKind::TTK_Struct;

    // Full qualified name of this record, including namespaces and template
    // specializations.
    SmallString<16> FullName;

    // When present, this record is a template or specialization.
    std::optional<TemplateInfo> Template;

    // Indicates if the record was declared using a typedef. Things like anonymous
    // structs in a typedef:
    //   typedef struct { ... } foo_t;
    // are converted into records with the typedef as the Name + this flag set.
    bool IsTypeDef = false;

    llvm::SmallVector<MemberTypeInfo, 4>
        Members;                             // List of info about record members.
    llvm::SmallVector<Reference, 4> Parents; // List of base/parent records
    // (does not include virtual parents).
    llvm::SmallVector<Reference, 4>
        VirtualParents; // List of virtual base/parent records.

    std::vector<BaseRecordInfo>
        Bases; // List of base/parent records; this includes inherited methods and
    // attributes

    ScopeChildren Children;
    AccessScope scope;
};

// Info for typedef and using statements.
struct TypedefInfo
    : public SymbolInfo
{
    TypedefInfo(
        SymbolID USR = SymbolID())
        : SymbolInfo(InfoType::IT_typedef, USR)
    {
    }

    void merge(TypedefInfo&& I);

    TypeInfo Underlying;

    // Inidicates if this is a new C++ "using"-style typedef:
    //   using MyVector = std::vector<int>
    // False means it's a C-style typedef:
    //   typedef std::vector<int> MyVector;
    bool IsUsing = false;
};

struct BaseRecordInfo : public RecordInfo {
    BaseRecordInfo();
    BaseRecordInfo(SymbolID USR, StringRef Name, StringRef Path, bool IsVirtual,
        AccessSpecifier Access, bool IsParent);

    // Indicates if base corresponds to a virtual inheritance
    bool IsVirtual = false;
    // Access level associated with this inherited info (public, protected,
    // private).
    AccessSpecifier Access = AccessSpecifier::AS_public;
    bool IsParent = false; // Indicates if this base is a direct parent
};

// Information for a single possible value of an enumeration.
struct EnumValueInfo {
    explicit EnumValueInfo(StringRef Name = StringRef(),
        StringRef Value = StringRef("0"),
        StringRef ValueExpr = StringRef())
        : Name(Name), Value(Value), ValueExpr(ValueExpr) {}

    bool operator==(const EnumValueInfo& Other) const {
        return std::tie(Name, Value, ValueExpr) ==
            std::tie(Other.Name, Other.Value, Other.ValueExpr);
    }

    SmallString<16> Name;

    // The computed value of the enumeration constant. This could be the result of
    // evaluating the ValueExpr, or it could be automatically generated according
    // to C rules.
    SmallString<16> Value;

    // Stores the user-supplied initialization expression for this enumeration
    // constant. This will be empty for implicit enumeration values.
    SmallString<16> ValueExpr;
};

// TODO: Expand to allow for documenting templating.
// Info for types.
struct EnumInfo : public SymbolInfo {
    EnumInfo() : SymbolInfo(InfoType::IT_enum) {}
    EnumInfo(SymbolID USR) : SymbolInfo(InfoType::IT_enum, USR) {}

    void merge(EnumInfo&& I);

    // Indicates whether this enum is scoped (e.g. enum class).
    bool Scoped = false;

    // Set to nonempty to the type when this is an explicitly typed enum. For
    //   enum Foo : short { ... };
    // this will be "short".
    std::optional<TypeInfo> BaseType;

    llvm::SmallVector<EnumValueInfo, 4> Members; // List of enum members.
};

//------------------------------------------------

struct Index
    : public Reference
{
    Index() = default;
    Index(
        StringRef Name)
        : Reference(SymbolID(), Name)
    {
    }

    Index(
        StringRef Name,
        StringRef JumpToSection)
        : Reference(SymbolID(), Name),
        JumpToSection(JumpToSection)
    {
    }

    Index(
        SymbolID USR,
        StringRef Name,
        InfoType IT,
        StringRef Path)
        : Reference(USR, Name, IT, Name, Path)
    {
    }

    // This is used to look for a USR in a vector of Indexes using std::find
    bool operator==(const SymbolID& Other) const
    {
        return USR == Other;
    }

    bool operator<(const Index& Other) const;

    std::optional<SmallString<16>> JumpToSection;
    std::vector<Index> Children;

    void sort();
};

// TODO: Add functionality to include separate markdown pages.

// A standalone function to call to merge a vector of infos into one.
// This assumes that all infos in the vector are of the same type, and will fail
// if they are different.
llvm::Expected<std::unique_ptr<Info>>
mergeInfos(std::vector<std::unique_ptr<Info>>& Values);

} // namespace mrdox
} // namespace clang

#endif
