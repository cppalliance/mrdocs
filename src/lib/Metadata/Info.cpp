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

#include "lib/Support/Radix.hpp"
#include "lib/Dom/LazyObject.hpp"
#include "lib/Dom/LazyArray.hpp"
#include <mrdocs/Metadata/Info.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Metadata/Record.hpp>
#include <clang/AST/Type.h>
#include <llvm/ADT/STLExtras.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

namespace clang {
namespace mrdocs {

dom::String
toString(InfoKind kind) noexcept
{
    switch(kind)
    {
#define INFO(PascalName, LowerName) \
    case InfoKind::PascalName: return #LowerName;
#include <mrdocs/Metadata/InfoNodesPascalAndLower.inc>
    default:
        MRDOCS_UNREACHABLE();
    }
}

/* Customization to map Info types to DOM objects

   This function maps an Info type to a DOM object.
   It includes all members of the derived type.

   The traits store a reference to the DomCorpus
   so that it can resolve symbol IDs to the corresponding
   Info objects.

   Whenever a member refers to symbol IDs,
   the mapping trait will automatically resolve the
   symbol ID to the corresponding Info object.

   This allows all references to be resolved to the
   corresponding Info object lazily from the templates
   that use the DOM.
 */
template <class IO, class InfoTy>
requires std::derived_from<InfoTy, Info>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    InfoTy const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(domCorpus);
    io.map("id", I.id);
    if (!I.Name.empty())
    {
        io.map("name", I.Name);
    }
    io.map("kind", I.Kind);
    io.map("access", I.Access);
    io.map("extraction", I.Extraction);
    io.map("isRegular", I.Extraction == ExtractionMode::Regular);
    io.map("isSeeBelow", I.Extraction == ExtractionMode::SeeBelow);
    io.map("isImplementationDefined", I.Extraction == ExtractionMode::ImplementationDefined);
    io.map("isDependency", I.Extraction == ExtractionMode::Dependency);
    if (I.Parent)
    {
        io.map("parent", I.Parent);
        io.defer("parents", [&]
        {
            // A convenient list to iterate over the parents
            // with resorting to partial template recursion
            Corpus const& corpus = domCorpus->getCorpus();
            auto pIds = getParents(corpus, I);
            dom::Array res;
            for (auto const& id : pIds)
            {
                Info const& PI = corpus.get(id);
                res.push_back(domCorpus->construct(PI));
            }
            return res;
        });
    }
    if (I.javadoc)
    {
        io.map("doc", *I.javadoc);
    }
    using T = std::remove_cvref_t<InfoTy>;
    if constexpr(std::derived_from<T, ScopeInfo>)
    {
        io.map("members", dom::LazyArray(I.Members, domCorpus));
        io.defer("overloads", [&]{
            // Eager array with overloadset or symbol
            return generateScopeOverloadsArray(I, *domCorpus);
        });
    }
    if constexpr(std::derived_from<T, SourceInfo>)
    {
        io.map("loc", static_cast<SourceInfo const&>(I));
    }
    if constexpr(T::isNamespace())
    {
        io.defer("interface", [&I, domCorpus]{
            // Eager object with each Info type
            auto t = std::make_shared<Tranche>(makeTranche(I, **domCorpus));
            return dom::ValueFrom(t, domCorpus);
        });
        io.map("usingDirectives", dom::LazyArray(I.UsingDirectives, domCorpus));
    }
    if constexpr (T::isRecord())
    {
        io.map("tag", I.KeyKind);
        io.map("defaultAccess", getDefaultAccessString(I.KeyKind));
        io.map("isTypedef", I.IsTypeDef);
        io.map("bases", dom::LazyArray(I.Bases, domCorpus));
        io.defer("interface", [domCorpus, &I] {
            // Eager object with each Info type for each access specifier
            auto sp = std::make_shared<Interface>(makeInterface(I, domCorpus->getCorpus()));
            return dom::ValueFrom(sp, domCorpus);
        });
        io.map("template", I.Template);
    }
    if constexpr (T::isEnum())
    {
        io.map("type", I.UnderlyingType);
        io.map("isScoped", I.Scoped);
    }
    if constexpr (T::isFunction())
    {
        io.map("isVariadic", I.IsVariadic);
        io.map("isVirtual", I.IsVirtual);
        io.map("isVirtualAsWritten", I.IsVirtualAsWritten);
        io.map("isPure", I.IsPure);
        io.map("isDefaulted", I.IsDefaulted);
        io.map("isExplicitlyDefaulted", I.IsExplicitlyDefaulted);
        io.map("isDeleted", I.IsDeleted);
        io.map("isDeletedAsWritten", I.IsDeletedAsWritten);
        io.map("isNoReturn", I.IsNoReturn);
        io.map("hasOverrideAttr", I.HasOverrideAttr);
        io.map("hasTrailingReturn", I.HasTrailingReturn);
        io.map("isConst", I.IsConst);
        io.map("isVolatile", I.IsVolatile);
        io.map("isFinal", I.IsFinal);
        io.map("isNodiscard", I.IsNodiscard);
        io.map("isExplicitObjectMemberFunction", I.IsExplicitObjectMemberFunction);
        if (I.Constexpr != ConstexprKind::None)
        {
            io.map("constexprKind", I.Constexpr);
        }
        if (I.StorageClass != StorageClassKind::None)
        {
            io.map("storageClass", I.StorageClass);
        }
        if (I.RefQualifier != ReferenceKind::None)
        {
            io.map("refQualifier", I.RefQualifier);
        }
        io.map("class", I.Class);
        io.map("params", dom::LazyArray(I.Params, domCorpus));
        io.map("return", I.ReturnType);
        io.map("template", I.Template);
        io.map("overloadedOperator", I.OverloadedOperator);
        io.map("exceptionSpec", I.Noexcept);
        io.map("explicitSpec", I.Explicit);
        if (!I.Requires.Written.empty())
        {
            io.map("requires", I.Requires.Written);
        }
        io.map("attributes", dom::LazyArray(I.Attributes));
    }
    if constexpr (T::isTypedef())
    {
        io.map("type", I.Type);
        io.map("template", I.Template);
        io.map("isUsing", I.IsUsing);
    }
    if constexpr (T::isVariable())
    {
        io.map("type", I.Type);
        io.map("template", I.Template);
        if (I.Constexpr != ConstexprKind::None)
        {
            io.map("constexprKind", I.Constexpr);
        }
        if (I.StorageClass != StorageClassKind::None)
        {
            io.map("storageClass", I.StorageClass);
        }
        io.map("isConstinit", I.IsConstinit);
        io.map("isThreadLocal", I.IsThreadLocal);
        if (!I.Initializer.Written.empty())
        {
            io.map("initializer", I.Initializer.Written);
        }
    }
    if constexpr (T::isField())
    {
        io.map("type", I.Type);
        if (!I.Default.Written.empty())
        {
            io.map("default", I.Default.Written);
        }
        io.map("isMaybeUnused", I.IsMaybeUnused);
        io.map("isDeprecated", I.IsDeprecated);
        io.map("isVariant", I.IsVariant);
        io.map("isMutable", I.IsMutable);
        io.map("isBitfield", I.IsBitfield);
        io.map("hasNoUniqueAddress", I.HasNoUniqueAddress);
        if (I.IsBitfield)
        {
            io.map("bitfieldWidth", I.BitfieldWidth.Written);
        }
        io.map("attributes", dom::LazyArray(I.Attributes));
    }
    if constexpr (T::isSpecialization())
    {}
    if constexpr (T::isFriend())
    {
        if (I.FriendSymbol)
        {
            io.defer("name", [&I, domCorpus]{
                return dom::ValueFrom(I.FriendSymbol, domCorpus).get("name");
            });
            io.map("symbol", I.FriendSymbol);
        }
        else if (I.FriendType)
        {
            io.defer("name", [&]{
                return dom::ValueFrom(I.FriendType, domCorpus).get("name");
            });
            io.map("type", I.FriendType);
        }
    }
    if constexpr (T::isNamespaceAlias())
    {
        MRDOCS_ASSERT(I.AliasedSymbol);
        io.map("aliasedSymbol", I.AliasedSymbol);
    }
    if constexpr (T::isUsing())
    {
        io.map("class", I.Class);
        io.map("shadows", dom::LazyArray(I.UsingSymbols, domCorpus));
        io.map("qualifier", I.Qualifier);
    }
    if constexpr (T::isEnumConstant())
    {
        if (!I.Initializer.Written.empty())
        {
            io.map("initializer", I.Initializer.Written);
        }
    }
    if constexpr (T::isGuide())
    {
        io.map("params", dom::LazyArray(I.Params, domCorpus));
        io.map("deduced", I.Deduced);
        io.map("template", I.Template);
        io.map("explicitSpec", I.Explicit);
    }
    if constexpr (T::isConcept())
    {
        io.map("template", I.Template);
        if (!I.Constraint.Written.empty())
        {
            io.map("constraint", I.Constraint.Written);
        }
    }
}

void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Info const& I,
    DomCorpus const* domCorpus)
{
    return visit(I,
        [&]<class T>(T const& I)
        {
            v = dom::LazyObject(I, domCorpus);
        });
}

bool
operator<(
    Info const& lhs,
    Info const& rhs) noexcept
{
    if (lhs.Kind != rhs.Kind)
    {
        return lhs.Kind < rhs.Kind;
    }
    if (lhs.Name != rhs.Name)
    {
        return lhs.Name < rhs.Name;
    }

    // If kind and name are the same, compare by template arguments
    TemplateInfo const* lhsTemplate = visit(lhs, [](auto const& U)
        -> TemplateInfo const*
    {
        if constexpr (requires { U.Template; })
        {
            if (U.Template)
            {
                return &*U.Template;
            }
        }
        return nullptr;
    });
    TemplateInfo const* rhsTemplate = visit(rhs, [](auto const& U)
        -> TemplateInfo const*
    {
        if constexpr (requires { U.Template; })
        {
            if (U.Template)
            {
                return &*U.Template;
            }
        }
        return nullptr;
    });
    if (!lhsTemplate && !rhsTemplate)
    {
        return false;
    }
    if (!lhsTemplate)
    {
        return true;
    }
    if (!rhsTemplate)
    {
        return false;
    }
    if (lhsTemplate->Args.size() != rhsTemplate->Args.size())
    {
        return lhsTemplate->Args.size() < rhsTemplate->Args.size();
    }
    for (std::size_t i = 0; i < lhsTemplate->Args.size(); ++i)
    {
        if (lhsTemplate->Args[i] != rhsTemplate->Args[i])
        {
            return lhsTemplate->Args[i]->Kind < rhsTemplate->Args[i]->Kind;
        }
    }
    return false;
}

} // mrdocs
} // clang
