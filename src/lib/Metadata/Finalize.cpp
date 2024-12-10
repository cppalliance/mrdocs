//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "Finalize.hpp"
#include "lib/Lib/Info.hpp"
#include "lib/Support/NameParser.hpp"
#include <mrdocs/Metadata.hpp>
#include <algorithm>
#include <ranges>
#include <span>

namespace clang {
namespace mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid are not checked.
*/
class Finalizer
{
    InfoSet& info_;
    SymbolLookup& lookup_;
    Info* current_ = nullptr;

    bool resolveReference(doc::Reference& ref)
    {
        auto parse_result = parseIdExpression(ref.string);
        if(! parse_result)
            return false;

        if(parse_result->name.empty())
            return false;

        auto is_acceptable = [&](const Info& I) -> bool
        {
            // if we are copying the documentation of the
            // referenced symbol, ignore the current declaration
            if(ref.kind == doc::Kind::copied)
                return &I != current_;
            // otherwise, consider the result to be acceptable
            return true;
        };

        const Info* found = nullptr;
        if(parse_result->qualified)
        {
            Info* context = current_;
            std::vector<std::string_view> qualifier;
            // KRYSTIAN FIXME: lookupQualified should accept
            // std::vector<std::string> as the qualifier
            for(auto& part : parse_result->qualifier)
                qualifier.push_back(part);
            if(parse_result->qualifier.empty())
            {
                MRDOCS_ASSERT(info_.contains(SymbolID::global));
                context = info_.find(SymbolID::global)->get();
            }
            found = lookup_.lookupQualified(
                context,
                qualifier,
                parse_result->name,
                is_acceptable);
        }
        else
        {
            found = lookup_.lookupUnqualified(
                current_,
                parse_result->name,
                is_acceptable);
        }

        // prevent recursive documentation copies
        if(ref.kind == doc::Kind::copied &&
            found && found->id == current_->id)
            return false;

        // if we found a symbol, replace the reference
        // ID with the SymbolID of that symbol
        if(found)
            ref.id = found->id;
        return found;
    }

    void finalize(SymbolID& id)
    {
        if(id && ! info_.contains(id))
            id = SymbolID::invalid;
    }

    void finalize(std::vector<SymbolID>& ids)
    {
        std::erase_if(ids, [this](const SymbolID& id)
        {
            return ! id || ! info_.contains(id);
        });
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
            if constexpr(Ty::isType())
                finalize(P.Constraint);

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

            if constexpr(Ty::isNamed())
                finalize(T.Name);

            if constexpr(Ty::isAuto())
                finalize(T.Constraint);
        });
    }

    void finalize(NameInfo& name)
    {
        visit(name, [this]<typename Ty>(Ty& T)
        {
            finalize(T.Prefix);

            if constexpr(requires { T.TemplateArgs; })
                finalize(T.TemplateArgs);

            finalize(T.id);
        });
    }

    void finalize(doc::Node& node)
    {
        visit(node, [&]<typename NodeTy>(NodeTy& N)
        {
            if constexpr(requires { N.children; })
                finalize(N.children);

            if constexpr(std::derived_from<NodeTy, doc::Reference>)
            {
#if 0
                // This warning shouldn't be triggered if the symbol has
                // been explicitly marked excluded in mrdocs.yml
                if(! resolveReference(N))
                {
                    report::warn("Failed to resolve reference to '{}' from '{}'",
                        N.string, current_->Name);
                }
#else
                resolveReference(N);
#endif
            }
        });
    }

    void finalize(Javadoc& javadoc)
    {
        finalize(javadoc.getBlocks());
    }

    template<typename T>
    void finalize(Optional<T>& val) requires
        requires { this->finalize(*val); }
    {
        if(val)
            finalize(*val);
    }

    template<typename T>
    void finalize(T* ptr) requires
        // KRYSTIAN NOTE: msvc doesn't treat finalize as a dependent
        // name unless part of a class member access...
        requires { this->finalize(*ptr); }
    {
        if(ptr)
            finalize(*ptr);
    }

    template<typename T>
    void finalize(std::unique_ptr<T>& ptr) requires
        requires { this->finalize(*ptr); }
    {
        if(ptr)
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
        MRDOCS_ASSERT(info_.contains(id));
    }

    void check(const std::vector<SymbolID>& ids)
    {
        MRDOCS_ASSERT(std::all_of(ids.begin(), ids.end(),
            [this](const SymbolID& id)
            {
                return info_.contains(id);
            }));
    }


public:
    Finalizer(
        InfoSet& Info,
        SymbolLookup& Lookup)
        : info_(Info)
        , lookup_(Lookup)
    {
    }

    void finalize(Info& I)
    {
        current_ = &I;
        visit(I, *this);
    }

    void operator()(NamespaceInfo& I)
    {
        check(I.Namespace);
        check(I.Members);
        finalize(I.javadoc);
        finalize(I.UsingDirectives);
        // finalize(I.Specializations);
    }

    void operator()(RecordInfo& I)
    {
        check(I.Namespace);
        check(I.Members);
        finalize(I.javadoc);
        // finalize(I.Specializations);
        finalize(I.Template);
        finalize(I.Bases);
    }

    void operator()(SpecializationInfo& I)
    {
        check(I.Namespace);
        check(I.Members);
        finalize(I.javadoc);
        finalize(I.Primary);
        finalize(I.Args);
    }

    void operator()(FunctionInfo& I)
    {
        check(I.Namespace);
        finalize(I.javadoc);
        finalize(I.Template);
        finalize(I.ReturnType);
        finalize(I.Params);
    }

    void operator()(TypedefInfo& I)
    {
        check(I.Namespace);
        finalize(I.javadoc);
        finalize(I.Template);
        finalize(I.Type);
    }

    void operator()(EnumInfo& I)
    {
        check(I.Namespace);
        check(I.Members);
        finalize(I.javadoc);
        finalize(I.UnderlyingType);
    }

    void operator()(FieldInfo& I)
    {
        check(I.Namespace);
        finalize(I.javadoc);
        finalize(I.Type);
    }

    void operator()(VariableInfo& I)
    {
        check(I.Namespace);
        finalize(I.javadoc);
        finalize(I.Template);
        finalize(I.Type);
    }

    void operator()(FriendInfo& I)
    {
        check(I.Namespace);
        finalize(I.javadoc);
        finalize(I.FriendSymbol);
        finalize(I.FriendType);
    }

    void operator()(NamespaceAliasInfo& I)
    {
        check(I.Namespace);
        finalize(I.javadoc);
        finalize(I.AliasedSymbol);
    }

    void operator()(UsingInfo& I)
    {
        check(I.Namespace);
        finalize(I.javadoc);
        finalize(I.Qualifier);
        finalize(I.UsingSymbols);
    }

    void operator()(EnumConstantInfo& I)
    {
        check(I.Namespace);
        finalize(I.javadoc);
    }

    void operator()(GuideInfo& I)
    {
        check(I.Namespace);
        finalize(I.javadoc);
        finalize(I.Template);
        finalize(I.Deduced);
        finalize(I.Params);
    }

    void operator()(ConceptInfo& I)
    {
        check(I.Namespace);
        finalize(I.javadoc);
        finalize(I.Template);
    }
};

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid are not checked.
*/
void
finalize(InfoSet& Info, SymbolLookup& Lookup)
{
    Finalizer visitor(Info, Lookup);
    for(auto& I : Info)
    {
        MRDOCS_ASSERT(I);
        visitor.finalize(*I);
    }
}

} // mrdocs
} // clang
