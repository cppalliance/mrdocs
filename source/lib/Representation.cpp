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
// This file defines the merging of different types of infos. The data in the
// calling Info is preserved during a merge unless that field is empty or
// default. In that case, the data from the parameter Info is used to replace
// the empty or default data.
//
// For most fields, the first decl seen provides the data. Exceptions to this
// include the location and description fields, which are collections of data on
// all decls related to a given definition. All other fields are ignored in new
// decls unless the first seen decl didn't, for whatever reason, incorporate
// data on that field (e.g. a forward declared class wouldn't have information
// on members on the forward declaration, but would have the class name).
//

#include "jad/Index.hpp"
#include "Reduce.h"
#include "Representation.h"
#include "jad/Namespace.hpp"
#include <mrdox/Config.hpp>
#include "llvm/Support/Error.h"
#include "llvm/Support/Path.h"

static_assert(clang::AccessSpecifier::AS_public == 0);
static_assert(clang::AccessSpecifier::AS_protected == 1);
static_assert(clang::AccessSpecifier::AS_private == 2);
static_assert(clang::AccessSpecifier::AS_none == 3);

namespace clang {
namespace mrdox {

// Dispatch function.
llvm::Expected<std::unique_ptr<Info>>
mergeInfos(std::vector<std::unique_ptr<Info>>& Values) {
    if (Values.empty() || !Values[0])
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "no info values to merge");

    switch (Values[0]->IT) {
    case InfoType::IT_namespace:
        return reduce<NamespaceInfo>(Values);
    case InfoType::IT_record:
        return reduce<RecordInfo>(Values);
    case InfoType::IT_enum:
        return reduce<EnumInfo>(Values);
    case InfoType::IT_function:
        return reduce<FunctionInfo>(Values);
    case InfoType::IT_typedef:
        return reduce<TypedefInfo>(Values);
    default:
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "unexpected info type");
    }
}

bool CommentInfo::operator==(const CommentInfo& Other) const {
    auto FirstCI = std::tie(Kind, Text, Name, Direction, ParamName, CloseName,
        SelfClosing, Explicit, AttrKeys, AttrValues, Args);
    auto SecondCI =
        std::tie(Other.Kind, Other.Text, Other.Name, Other.Direction,
            Other.ParamName, Other.CloseName, Other.SelfClosing,
            Other.Explicit, Other.AttrKeys, Other.AttrValues, Other.Args);

    if (FirstCI != SecondCI || Children.size() != Other.Children.size())
        return false;

    return std::equal(Children.begin(), Children.end(), Other.Children.begin(),
        llvm::deref<std::equal_to<>>{});
}

bool CommentInfo::operator<(const CommentInfo& Other) const {
    auto FirstCI = std::tie(Kind, Text, Name, Direction, ParamName, CloseName,
        SelfClosing, Explicit, AttrKeys, AttrValues, Args);
    auto SecondCI =
        std::tie(Other.Kind, Other.Text, Other.Name, Other.Direction,
            Other.ParamName, Other.CloseName, Other.SelfClosing,
            Other.Explicit, Other.AttrKeys, Other.AttrValues, Other.Args);

    if (FirstCI < SecondCI)
        return true;

    if (FirstCI == SecondCI) {
        return std::lexicographical_compare(
            Children.begin(), Children.end(), Other.Children.begin(),
            Other.Children.end(), llvm::deref<std::less<>>());
    }

    return false;
}

void EnumInfo::merge(EnumInfo&& Other) {
    assert(mergeable(Other));
    if (!Scoped)
        Scoped = Other.Scoped;
    if (Members.empty())
        Members = std::move(Other.Members);
    SymbolInfo::merge(std::move(Other));
}

void FunctionInfo::merge(FunctionInfo&& Other) {
    assert(mergeable(Other));
    if (!IsMethod)
        IsMethod = Other.IsMethod;
    if (!Access)
        Access = Other.Access;
    if (ReturnType.Type.USR == EmptySID && ReturnType.Type.Name == "")
        ReturnType = std::move(Other.ReturnType);
    if (Parent.USR == EmptySID && Parent.Name == "")
        Parent = std::move(Other.Parent);
    if (Params.empty())
        Params = std::move(Other.Params);
    SymbolInfo::merge(std::move(Other));
    if (!Template)
        Template = Other.Template;
}

void TypedefInfo::merge(TypedefInfo&& Other) {
    assert(mergeable(Other));
    if (!IsUsing)
        IsUsing = Other.IsUsing;
    if (Underlying.Type.Name == "")
        Underlying = Other.Underlying;
    SymbolInfo::merge(std::move(Other));
}

BaseRecordInfo::BaseRecordInfo() : RecordInfo() {}

BaseRecordInfo::BaseRecordInfo(SymbolID USR, StringRef Name, StringRef Path,
    bool IsVirtual, AccessSpecifier Access,
    bool IsParent)
    : RecordInfo(USR, Name, Path), IsVirtual(IsVirtual), Access(Access),
    IsParent(IsParent) {}

// Order is based on the Name attribute:
// case insensitive order
bool
Index::operator<(const Index& Other) const {
    // Loop through each character of both strings
    for (unsigned I = 0; I < Name.size() && I < Other.Name.size(); ++I) {
        // Compare them after converting both to lower case
        int D = tolower(Name[I]) - tolower(Other.Name[I]);
        if (D == 0)
            continue;
        return D < 0;
    }
    // If both strings have the size it means they would be equal if changed to
    // lower case. In here, lower case will be smaller than upper case
    // Example: string < stRing = true
    // This is the opposite of how operator < handles strings
    if (Name.size() == Other.Name.size())
        return Name > Other.Name;
    // If they are not the same size; the shorter string is smaller
    return Name.size() < Other.Name.size();
}

void Index::sort() {
    llvm::sort(Children);
    for (auto& C : Children)
        C.sort();
}

} // mrdox
} // clang
