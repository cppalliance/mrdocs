//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ReferenceFinalizer.hpp"
#include "lib/Support/NameParser.hpp"

namespace clang::mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid
    are not checked.
*/
bool
ReferenceFinalizer::
resolveReference(doc::Reference& ref) const
{
    auto parse_result = parseIdExpression(ref.string);
    if (!parse_result)
    {
        return false;
    }

    if (parse_result->name.empty())
    {
        return false;
    }

    auto is_acceptable = [&](Info const& I) -> bool
    {
        // if we are copying the documentation of the
        // referenced symbol, ignore the current declaration
        if (ref.kind == doc::Kind::copied)
        {
            return &I != current_;
        }
        // otherwise, consider the result to be acceptable
        return true;
    };

    Info const* found = nullptr;
    if(parse_result->qualified)
    {
        Info* context = current_;
        std::vector<std::string_view> qualifier;
        // KRYSTIAN FIXME: lookupQualified should accept
        // std::vector<std::string> as the qualifier
        for (auto& part: parse_result->qualifier)
        {
            qualifier.push_back(part);
        }
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
    if (ref.kind == doc::Kind::copied &&
        found &&
        found->id == current_->id)
    {
        return false;
    }

    // if we found a symbol, replace the reference
    // ID with the SymbolID of that symbol
    if (found)
    {
        ref.id = found->id;
    }
    return found;
}

void
ReferenceFinalizer::
finalize(SymbolID& id)
{
    if (id && !info_.contains(id))
    {
        id = SymbolID::invalid;
    }
}

void
ReferenceFinalizer::
finalize(std::vector<SymbolID>& ids)
{
    std::erase_if(ids, [this](SymbolID const& id)
    {
        return !id || ! info_.contains(id);
    });
}

void
ReferenceFinalizer::
finalize(TArg& arg)
{
    visit(arg, [this]<typename Ty>(Ty& A)
    {
        if constexpr (Ty::isType())
        {
            finalize(A.Type);
        }
        if constexpr (Ty::isTemplate())
        {
            finalize(A.Template);
        }
    });
}

void
ReferenceFinalizer::
finalize(TParam& param)
{
    finalize(param.Default);

    visit(param, [this]<typename Ty>(Ty& P)
    {
        if constexpr (Ty::isType())
        {
            finalize(P.Constraint);
        }

        if constexpr (Ty::isNonType())
        {
            finalize(P.Type);
        }

        if constexpr (Ty::isTemplate())
        {
            finalize(P.Params);
        }
    });
}

void
ReferenceFinalizer::
finalize(Param& param)
{
    finalize(param.Type);
}

void
ReferenceFinalizer::
finalize(BaseInfo& info)
{
    finalize(info.Type);
}

void
ReferenceFinalizer::
finalize(TemplateInfo& info)
{
    finalize(info.Args);
    finalize(info.Params);
    finalize(info.Primary);
}

void
ReferenceFinalizer::
finalize(TypeInfo& type)
{
    finalize(type.innerType());

    visit(type, [this]<typename Ty>(Ty& T)
    {
        if constexpr (requires { T.ParentType; })
        {
            finalize(T.ParentType);
        }

        if constexpr (Ty::isNamed())
        {
            finalize(T.Name);
        }

        if constexpr (Ty::isAuto())
        {
            finalize(T.Constraint);
        }
    });
}

void
ReferenceFinalizer::
finalize(NameInfo& name)
{
    visit(name, [this]<typename Ty>(Ty& T)
    {
        finalize(T.Prefix);

        if constexpr(requires { T.TemplateArgs; })
            finalize(T.TemplateArgs);

        finalize(T.id);
    });
}

void
ReferenceFinalizer::
finalize(doc::Node& node)
{
    visit(node, [&]<typename NodeTy>(NodeTy& N)
    {
        if constexpr (requires { N.children; })
        {
            finalize(N.children);
        }

        if constexpr(std::derived_from<NodeTy, doc::Reference>)
        {
            if (!resolveReference(N))
            {
                // The warning shouldn't be triggered if the symbol name
                // has been explicitly marked excluded in mrdocs.yml.
                // Finalizer needs to be updated to handle this case.
                // When tagfile support is implemented, we can't
                // report an error if the reference exists in the tagfile.
                // if constexpr (false)
                // {
                //    report::warn(
                //        "Failed to resolve reference to '{}' from '{}'",
                //        N.string,
                //        current_->Name);
                // }
            }
        }
    });
}

void
ReferenceFinalizer::
finalize(Javadoc& javadoc)
{
    finalize(javadoc.getBlocks());
}

void
ReferenceFinalizer::
check(SymbolID const& id) const
{
    MRDOCS_ASSERT(info_.contains(id));
}

void
ReferenceFinalizer::
finalize(Info& I)
{
    current_ = &I;
    visit(I, *this);
}

void
ReferenceFinalizer::
operator()(NamespaceInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    check(allMembers(I));
    finalize(I.javadoc);
    finalize(I.UsingDirectives);
}

void
ReferenceFinalizer::
operator()(RecordInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    check(allMembers(I));
    finalize(I.javadoc);
    // finalize(I.Specializations);
    finalize(I.Template);
    finalize(I.Bases);
}

void
ReferenceFinalizer::
operator()(SpecializationInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    finalize(I.javadoc);
    finalize(I.Primary);
    finalize(I.Args);
}

void
ReferenceFinalizer::
operator()(FunctionInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    finalize(I.javadoc);
    finalize(I.Template);
    finalize(I.ReturnType);
    finalize(I.Params);
}

void
ReferenceFinalizer::
operator()(TypedefInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    finalize(I.javadoc);
    finalize(I.Template);
    finalize(I.Type);
}

void
ReferenceFinalizer::
operator()(EnumInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    check(allMembers(I));
    finalize(I.javadoc);
    finalize(I.UnderlyingType);
}

void
ReferenceFinalizer::
operator()(FieldInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    finalize(I.javadoc);
    finalize(I.Type);
}

void
ReferenceFinalizer::
operator()(VariableInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    finalize(I.javadoc);
    finalize(I.Template);
    finalize(I.Type);
}

void
ReferenceFinalizer::
operator()(FriendInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    finalize(I.javadoc);
    finalize(I.FriendSymbol);
    finalize(I.FriendType);
}

void
ReferenceFinalizer::
operator()(NamespaceAliasInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    finalize(I.javadoc);
    finalize(I.AliasedSymbol);
}

void
ReferenceFinalizer::
operator()(UsingInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    finalize(I.javadoc);
    finalize(I.Qualifier);
    finalize(I.UsingSymbols);
}

void
ReferenceFinalizer::
operator()(EnumConstantInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    finalize(I.javadoc);
}

void
ReferenceFinalizer::
operator()(GuideInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    finalize(I.javadoc);
    finalize(I.Template);
    finalize(I.Deduced);
    finalize(I.Params);
}

void
ReferenceFinalizer::
operator()(ConceptInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    finalize(I.javadoc);
    finalize(I.Template);
}

void
ReferenceFinalizer::
operator()(OverloadsInfo& I)
{
    if (I.Parent)
    {
        check(I.Parent);
    }
    check(allMembers(I));
    finalize(I.javadoc);
}

} // clang::mrdocs
