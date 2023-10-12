//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "Finalize.hpp"
#include "lib/Lib/Info.hpp"
#include <mrdox/Metadata.hpp>
#include <ranges>

namespace clang {
namespace mrdox {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid are not checked.
*/
class Finalizer
{
    InfoSet& info_;

    void finalize(SymbolID& id)
    {
        if(! info_.contains(id))
            id = SymbolID::zero;
    }

    void finalize(std::vector<SymbolID>& ids)
    {
        std::erase_if(ids, [this](const SymbolID& id)
        {
            return ! info_.contains(id);
        });
    }

    void finalize(SpecializedMember& member)
    {
        finalize(member.Primary);
        finalize(member.Specialized);
    }

    void finalize(TArg& arg)
    {
        visit(arg, [this]<typename Ty>(Ty& A)
        {
            if constexpr(Ty::isType())
                finalize(A.Type);

            if constexpr(Ty::isTemplate())
                finalize(A.Template);
        });
    }

    void finalize(TParam& param)
    {
        finalize(param.Default);

        visit(param, [this]<typename Ty>(Ty& P)
        {
            if constexpr(Ty::isNonType())
                finalize(P.Type);

            if constexpr(Ty::isTemplate())
                finalize(P.Params);
        });
    }

    void finalize(Param& param)
    {
        finalize(param.Type);
    }

    void finalize(BaseInfo& info)
    {
        finalize(info.Type);
    }

    void finalize(TemplateInfo& info)
    {
        finalize(info.Args);
        finalize(info.Params);
        finalize(info.Primary);
    }

    void finalize(TypeInfo& type)
    {
        finalize(type.innerType());

        visit(type, [this]<typename Ty>(Ty& T)
        {
            if constexpr(requires { T.ParentType; })
                finalize(T.ParentType);

            if constexpr(requires { T.id; })
                finalize(T.id);

            if constexpr(Ty::isSpecialization())
                finalize(T.TemplateArgs);
        });
    }

    template<typename T>
    void finalize(Optional<T>& val) requires
        requires { this->finalize(*val); }
    {
        if(! val)
            return;
        finalize(*val);
    }

    template<typename T>
    void finalize(T* ptr) requires
        // KRYSTIAN NOTE: msvc doesn't treat finalize as a dependent
        // name unless part of a class member access...
        requires { this->finalize(*ptr); }
    {
        if(! ptr)
            return;
        finalize(*ptr);
    }

    template<typename T>
    void finalize(std::unique_ptr<T>& ptr) requires
        requires { this->finalize(*ptr); }
    {
        if(! ptr)
            return;
        finalize(*ptr);
    }

    template<typename Range>
        requires std::ranges::input_range<Range>
    void finalize(Range&& range)
    {
        for(auto&& elem : range)
            finalize(elem);
    }

    // ----------------------------------------------------------------

    void check(const SymbolID& id)
    {
        MRDOX_ASSERT(info_.contains(id));
    }

    void check(const std::vector<SymbolID>& ids)
    {
        MRDOX_ASSERT(std::all_of(ids.begin(), ids.end(),
            [this](const SymbolID& id)
            {
                return info_.contains(id);
            }));
    }


public:
    Finalizer(InfoSet& IS)
        : info_(IS)
    {
    }

    void operator()(NamespaceInfo& I)
    {
        check(I.Namespace);
        check(I.Members);
        finalize(I.Specializations);
    }

    void operator()(RecordInfo& I)
    {
        check(I.Namespace);
        check(I.Members);
        finalize(I.Specializations);
        finalize(I.Template);
        finalize(I.Bases);
        finalize(I.Friends);
    }

    void operator()(SpecializationInfo& I)
    {
        check(I.Namespace);
        finalize(I.Members);
        finalize(I.Primary);
        finalize(I.Args);
    }

    void operator()(FunctionInfo& I)
    {
        check(I.Namespace);
        finalize(I.Template);
        finalize(I.ReturnType);
        finalize(I.Params);
    }

    void operator()(TypedefInfo& I)
    {
        check(I.Namespace);
        finalize(I.Template);
        finalize(I.Type);
    }

    void operator()(EnumInfo& I)
    {
        check(I.Namespace);
        finalize(I.UnderlyingType);
    }

    void operator()(FieldInfo& I)
    {
        check(I.Namespace);
        finalize(I.Type);
    }

    void operator()(VariableInfo& I)
    {
        check(I.Namespace);
        finalize(I.Template);
        finalize(I.Type);
    }
};

void finalize(InfoSet& IS)
{
    Finalizer visitor(IS);
    for(auto& I : IS)
        visit(*I, visitor);
}

} // mrdox
} // clang
