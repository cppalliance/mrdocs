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

#ifndef MRDOX_LIB_METADATA_REDUCE_HPP
#define MRDOX_LIB_METADATA_REDUCE_HPP

#include <mrdox/Metadata/Info.hpp>
#include <mrdox/MetadataFwd.hpp>
#include <llvm/Support/Error.h>
#include <memory>
#include <vector>

namespace clang {
namespace mrdox {

void merge(NamespaceInfo& I, NamespaceInfo&& Other);
void merge(RecordInfo& I, RecordInfo&& Other);
void merge(FunctionInfo& I, FunctionInfo&& Other);
void merge(TypedefInfo& I, TypedefInfo&& Other);
void merge(EnumInfo& I, EnumInfo&& Other);
void merge(FieldInfo& I, FieldInfo&& Other);
void merge(VarInfo& I, VarInfo&& Other);
void merge(Reference& I, Reference&& Other);

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

template <typename T>
llvm::Expected<std::unique_ptr<Info>>
reduce(
    std::vector<std::unique_ptr<Info>>& Values)
{
    if (Values.empty() || !Values[0])
        return llvm::createStringError(llvm::inconvertibleErrorCode(),
            "no value to reduce");
    std::unique_ptr<Info> Merged = std::make_unique<T>(Values[0]->id);
    T* Tmp = static_cast<T*>(Merged.get());
    for (auto& I : Values)
        merge(*Tmp, std::move(*static_cast<T*>(I.get())));
    return std::move(Merged);
}

// Return the index of the matching child in the list,
// or -1 if merge is not necessary.
template <typename T>
int
getChildIndexIfExists(
    std::vector<T>& Children,
    T& ChildToMerge)
{
    for (unsigned long I = 0; I < Children.size(); I++)
    {
        if (ChildToMerge.id == Children[I].id)
            return I;
    }
    return -1;
}

template<typename T>
void
reduceChildren(
    std::vector<T>& Children,
    std::vector<T>&& ChildrenToMerge)
{
    for (auto& ChildToMerge : ChildrenToMerge)
    {
        int MergeIdx = getChildIndexIfExists(Children, ChildToMerge);
        if (MergeIdx == -1) {
            Children.push_back(std::move(ChildToMerge));
            continue;
        }
        merge(Children[MergeIdx], std::move(ChildToMerge));
    }
}

} // mrdox
} // clang


#endif