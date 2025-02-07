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

#include "JavadocFinalizer.hpp"
#include <mrdocs/Support/ScopeExit.hpp>

namespace clang::mrdocs {

template <class InfoTy>
void
JavadocFinalizer::
operator()(InfoTy& I)
{
    MRDOCS_CHECK_OR(!finalized_.contains(&I));
    finalized_.emplace(&I);

    report::trace(
        "Finalizing javadoc for '{}'",
        corpus_.Corpus::qualifiedName(I));

    ScopeExitRestore s(current_context_, &I);

#ifndef NDEBUG
    // Check references
    if (I.Parent)
    {
        checkExists(I.Parent);
    }
    if constexpr (InfoParent<InfoTy>)
    {
        checkExists(allMembers(I));
    }
#endif

    // Finalize references in javadoc
    finalize(I.javadoc);

    if constexpr (requires { I.UsingDirectives; })
    {
        finalize(I.UsingDirectives);
    }
    if constexpr (requires { I.Template; })
    {
        finalize(I.Template);
    }
    if constexpr (requires { I.Bases; })
    {
        finalize(I.Bases);
    }
    if constexpr (requires { I.Primary; })
    {
        finalize(I.Primary);
    }
    if constexpr (requires { I.Args; })
    {
        finalize(I.Args);
    }
    if constexpr (requires { I.ReturnType; })
    {
        finalize(I.ReturnType);
    }
    if constexpr (requires { I.Params; })
    {
        finalize(I.Params);
    }
    if constexpr (requires { I.Type; })
    {
        finalize(I.Type);
    }
    if constexpr (requires { I.UnderlyingType; })
    {
        finalize(I.UnderlyingType);
    }
    if constexpr (requires { I.FriendSymbol; })
    {
        finalize(I.FriendSymbol);
    }
    if constexpr (requires { I.FriendType; })
    {
        finalize(I.FriendType);
    }
    if constexpr (requires { I.AliasedSymbol; })
    {
        finalize(I.AliasedSymbol);
    }
    if constexpr (requires { I.Qualifier; })
    {
        finalize(I.Qualifier);
    }
    if constexpr (requires { I.UsingSymbols; })
    {
        finalize(I.UsingSymbols);
    }
    if constexpr (requires { I.Deduced; })
    {
        finalize(I.Deduced);
    }
}

#define INFO(T) template void JavadocFinalizer::operator()<T##Info>(T##Info&);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

void
JavadocFinalizer::
finalize(doc::Reference& ref)
{
    Info const* res = corpus_.lookup(current_context_->id, ref.string);
    if (res)
    {
        ref.id = res->id;
    }
    if (res == nullptr &&
        // Only warn once per reference
        !warned_.contains({ref.string, current_context_->Name}) &&
        // Ignore std:: references
        !ref.string.starts_with("std::") &&
        // Ignore other reference types that can't be replaced by `...`
        ref.Kind == doc::NodeKind::reference)
    {
        MRDOCS_ASSERT(current_context_);
        if (auto primaryLoc = getPrimaryLocation(*current_context_))
        {
            report::warn(
                "{}:{}\n{}: Failed to resolve reference to '{}'",
                primaryLoc->FullPath,
                primaryLoc->LineNumber,
                corpus_.Corpus::qualifiedName(*current_context_),
                ref.string);
        }
        warned_.insert({ref.string, current_context_->Name});
    }
}

void
JavadocFinalizer::
finalize(doc::Node& node)
{
    visit(node, [&]<typename NodeTy>(NodeTy& N)
    {
        if constexpr (requires { N.children; })
        {
            finalize(N.children);
        }

        if constexpr(std::same_as<NodeTy, doc::Reference>)
        {
            finalize(dynamic_cast<doc::Reference&>(N));
        }
        else if constexpr (std::same_as<NodeTy, doc::Throws>)
        {
            finalize(dynamic_cast<doc::Throws&>(N).exception);
        }
    });
}

void
JavadocFinalizer::
finalize(Javadoc& javadoc)
{
    finalize(javadoc.blocks);
    finalize(javadoc.brief);
    finalize(javadoc.returns);
    finalize(javadoc.params);
    finalize(javadoc.tparams);
    finalize(javadoc.exceptions);
    finalize(javadoc.sees);
    finalize(javadoc.preconditions);
    finalize(javadoc.postconditions);
    copyBriefAndDetails(javadoc);
    setAutoBrief(javadoc);
}

void
JavadocFinalizer::
copyBriefAndDetails(Javadoc& javadoc)
{
    for (auto blockIt = javadoc.blocks.begin(); blockIt != javadoc.blocks.end();)
    {
        auto& block = *blockIt;
        if (block->Kind != doc::NodeKind::paragraph &&
            block->Kind != doc::NodeKind::details)
        {
            ++blockIt;
            continue;
        }
        auto& para = dynamic_cast<doc::Paragraph&>(*block);
        if (para.children.empty())
        {
            ++blockIt;
            continue;
        }
        // Find copydoc command
        std::optional<doc::Copied> copied;
        for (auto textIt = para.children.begin(); textIt != para.children.end();)
        {
            // Find copydoc command
            auto& text = *textIt;
            if (text->Kind != doc::NodeKind::copied)
            {
                ++textIt;
                continue;
            }
            // Copy reference
            copied = dynamic_cast<doc::Copied&>(*text);

            // Remove copied node
            /* it2 = */ para.children.erase(textIt);
            break;
        }
        // Remove leading children from the paragraph that are
        // either empty or only white spaces. We also ltrim
        // the first child with content.
        while (!para.children.empty())
        {
            if (para.children.front()->string.find_first_not_of(" \t\n\v\f\r") == std::string::npos)
            {
                para.children.erase(para.children.begin());
            }
            else
            {
                para.children.front()->string = ltrim(para.children.front()->string);
                break;
            }
        }
        // Remove trailing children from the paragraph that are
        // either empty or only white spaces. We also rtrim
        // the last child with content.
        while (!para.children.empty())
        {
            if (para.children.back()->string.find_first_not_of(" \t\n\v\f\r") == std::string::npos)
            {
                para.children.pop_back();
            }
            else
            {
                para.children.back()->string = rtrim(para.children.back()->string);
                break;
            }
        }
        // Remove empty completely empty children from the paragraph
        std::erase_if(para.children, [](Polymorphic<doc::Text> const& child) {
            return child->string.empty();
        });
        // Merge consecutive text nodes that have exactly the same terminal kind
        for (auto textIt = para.children.begin(); textIt != para.children.end();)
        {
            auto& text = *textIt;
            if (textIt != para.children.begin())
            {
                if (auto& prev = *std::prev(textIt);
                    prev->Kind == text->Kind)
                {
                    prev->string += text->string;
                    textIt = para.children.erase(textIt);
                    continue;
                }
            }
            ++textIt;
        }
        // Remove the entire paragraph block from the javadoc if it is empty
        if (para.empty())
        {
            blockIt = javadoc.blocks.erase(blockIt);
            MRDOCS_CHECK_OR_CONTINUE(copied);
        }
        if (!copied)
        {
            ++blockIt;
            continue;
        }

        // Find the node to copy from
        Info const* res = corpus_.lookup(current_context_->id, copied->string);
        if (!res)
        {
            MRDOCS_ASSERT(current_context_);
            if (auto primaryLoc = getPrimaryLocation(*current_context_))
            {
                report::warn(
                    "{}:{}\n"
                    "{}: Failed to copy documentation from '{}'\n"
                    "    Note: Symbol '{}' not found.",
                    primaryLoc->FullPath,
                    primaryLoc->LineNumber,
                    corpus_.Corpus::qualifiedName(*current_context_),
                    copied->string,
                    copied->string);
            }
            continue;
        }

        // Ensure the source node is finalized
        if (!finalized_.contains(res))
        {
            operator()(const_cast<Info&>(*res));
        }
        if (!res->javadoc)
        {
            auto ctxPrimaryLoc = getPrimaryLocation(*current_context_);
            auto resPrimaryLoc = getPrimaryLocation(*res);
            report::warn(
                "{}:{}\n"
                "{}: Failed to copy documentation from '{}'.\n"
                "No documentation available.\n"
                "    {}:{}\n"
                "    Note: No documentation available for '{}'.",
                ctxPrimaryLoc->FullPath,
                ctxPrimaryLoc->LineNumber,
                corpus_.Corpus::qualifiedName(*current_context_),
                copied->string,
                resPrimaryLoc->FullPath,
                resPrimaryLoc->LineNumber,
                corpus_.Corpus::qualifiedName(*res));
            continue;
        }

        // Copy brief and details
        bool const copyBrief = copied->parts == doc::Parts::all || copied->parts == doc::Parts::brief;
        bool const copyDetails = copied->parts == doc::Parts::all || copied->parts == doc::Parts::description;
        Javadoc const& src = *res->javadoc;
        if (copyBrief && !javadoc.brief)
        {
            javadoc.brief = src.brief;
        }
        if (copyDetails && !src.blocks.empty())
        {
            blockIt = javadoc.blocks.insert(blockIt, src.blocks.begin(), src.blocks.end());
            blockIt = std::next(blockIt, src.blocks.size());
            continue;
        }
        // Erasing the paragraph could make the iterator == end()
        if (blockIt != javadoc.blocks.end())
        {
            ++blockIt;
        }
    }
}

void
JavadocFinalizer::
setAutoBrief(Javadoc& javadoc)
{
    MRDOCS_CHECK_OR(corpus_.config->autoBrief);
    MRDOCS_CHECK_OR(!javadoc.brief);
    MRDOCS_CHECK_OR(!javadoc.blocks.empty());
    for (auto it = javadoc.blocks.begin(); it != javadoc.blocks.end();)
    {
        if (auto& block = *it;
            block->Kind == doc::NodeKind::paragraph ||
            block->Kind == doc::NodeKind::details)
        {
            auto& para = dynamic_cast<doc::Paragraph&>(*block);
            if (para.children.empty())
            {
                ++it;
                continue;
            }
            javadoc.brief.emplace();
            javadoc.brief->children = para.children;
            it = javadoc.blocks.erase(it);
            return;
        }
        else
        {
            ++it;
            continue;
        }
    }
}


void
JavadocFinalizer::
finalize(SymbolID& id)
{
    if (id && !corpus_.info_.contains(id))
    {
        id = SymbolID::invalid;
    }
}

void
JavadocFinalizer::
finalize(std::vector<SymbolID>& ids)
{
    std::erase_if(ids, [this](SymbolID const& id)
    {
        return !id || ! corpus_.info_.contains(id);
    });
}

void
JavadocFinalizer::
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
JavadocFinalizer::
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
JavadocFinalizer::
finalize(Param& param)
{
    finalize(param.Type);
}

void
JavadocFinalizer::
finalize(BaseInfo& info)
{
    finalize(info.Type);
}

void
JavadocFinalizer::
finalize(TemplateInfo& info)
{
    finalize(info.Args);
    finalize(info.Params);
    finalize(info.Primary);
}

void
JavadocFinalizer::
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
JavadocFinalizer::
finalize(NameInfo& name)
{
    visit(name, [this]<typename Ty>(Ty& T)
    {
        finalize(T.Prefix);
        if constexpr(requires { T.TemplateArgs; })
        {
            finalize(T.TemplateArgs);
        }
        finalize(T.id);
    });
}

void
JavadocFinalizer::
checkExists(SymbolID const& id) const
{
    MRDOCS_ASSERT(corpus_.info_.contains(id));
}

} // clang::mrdocs
