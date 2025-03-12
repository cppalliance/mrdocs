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
#include <mrdocs/Support/Algorithm.hpp>

namespace clang::mrdocs {

template <class InfoTy>
void
JavadocFinalizer::
operator()(InfoTy& I)
{
    MRDOCS_CHECK_OR(!finalized_.contains(&I));
    finalized_.emplace(&I);

    if constexpr (InfoTy::isOverloads())
    {
        if (!I.javadoc)
        {
            populateOverloadJavadocs(I);
        }
    }

    report::trace(
        "Finalizing javadoc for '{}'",
        corpus_.Corpus::qualifiedName(I));

    ScopeExitRestore s(current_context_, &I);

    // Finalize references in javadoc
    finalize(I.javadoc);

    finalizeInfoData(I);
}

#define INFO(T) template void JavadocFinalizer::operator()<T## Info>(T## Info&);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

template <class InfoTy>
void
JavadocFinalizer::
finalizeInfoData(InfoTy& I)
{
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

#define INFO(T) template void JavadocFinalizer::finalizeInfoData<T## Info>(T## Info&);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

void
JavadocFinalizer::
finalize(doc::Reference& ref, bool const emitWarning)
{
    if (auto resRef = corpus_.lookup(current_context_->id, ref.string))
    {
        // KRYSTIAN NOTE: we should provide an overload that
        // returns a non-const reference.
        auto& res = const_cast<Info&>(resRef->get());
        ref.id = res.id;
    }
    else if (
        emitWarning &&
        corpus_.config->warnings &&
        corpus_.config->warnBrokenRef &&
        // Only warn once per reference
        !refWarned_.contains({ref.string, current_context_->Name}) &&
        // Ignore std:: references
        !ref.string.starts_with("std::") &&
        // Ignore other reference types that can't be replaced by `...`
        ref.Kind == doc::NodeKind::reference)
    {
        MRDOCS_ASSERT(current_context_);
        this->warn(
            "{}: Failed to resolve reference to '{}'\n"
            "    {}",
            corpus_.Corpus::qualifiedName(*current_context_),
            ref.string,
            resRef.error().reason());
        refWarned_.insert({ref.string, current_context_->Name});
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
            finalize(dynamic_cast<doc::Reference&>(N), true);
        }
        else if constexpr (std::same_as<NodeTy, doc::Throws>)
        {
            finalize(dynamic_cast<doc::Throws&>(N).exception, false);
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
    setRelateIds(javadoc);
    copyBriefAndDetails(javadoc);
    setAutoBrief(javadoc);
    removeTempTextNodes(javadoc);
    trimBlocks(javadoc);
    unindentCodeBlocks(javadoc);
}

void
JavadocFinalizer::
setRelateIds(Javadoc& javadoc)
{
    MRDOCS_CHECK_OR(!javadoc.relates.empty());

    Info const* currentPtr = corpus_.find(current_context_->id);
    MRDOCS_ASSERT(currentPtr);
    Info const current = *currentPtr;

    if (!current.isFunction())
    {
        this->warn(
            "{}: `@relates` only allowed for functions",
            corpus_.Corpus::qualifiedName(current));
        javadoc.relates.clear();
        return;
    }

    for (doc::Reference& ref: javadoc.relates)
    {
        finalize(ref, true);
        Info* relatedPtr = corpus_.find(ref.id);
        MRDOCS_CHECK_OR_CONTINUE(relatedPtr);
        Info& related = *relatedPtr;
        if (!related.javadoc)
        {
            related.javadoc.emplace();
        }
        if (std::ranges::none_of(
                related.javadoc->related,
                [this](doc::Reference const& otherRef) {
            return otherRef.id == current_context_->id;
        }))
        {
            std::string currentName = corpus_.Corpus::qualifiedName(current);
            doc::Reference relatedRef(std::move(currentName));
            relatedRef.id = current_context_->id;
            related.javadoc->related.push_back(std::move(relatedRef));
        }
    }

    // Erase anything in the javadoc without a valid id
    std::erase_if(javadoc.relates, [](doc::Reference const& ref) {
        return !ref.id;
    });
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
        auto resRef = corpus_.lookup(current_context_->id, copied->string);
        if (!resRef)
        {
            if (corpus_.config->warnings &&
                corpus_.config->warnBrokenRef &&
                !refWarned_.contains({copied->string, current_context_->Name}))
            {
                this->warn(
                    "{}: Failed to copy documentation from '{}' (symbol not found)\n"
                    "    {}",
                    corpus_.Corpus::qualifiedName(*current_context_),
                    copied->string,
                    resRef.error().reason());
            }
            continue;
        }

        // Ensure the source node is finalized
        Info const& res = *resRef;
        if (!finalized_.contains(&res))
        {
            operator()(const_cast<Info&>(res));
        }
        if (!res.javadoc)
        {
            if (corpus_.config->warnings &&
                corpus_.config->warnBrokenRef &&
                !refWarned_.contains({copied->string, current_context_->Name}))
            {
                auto resPrimaryLoc = getPrimaryLocation(res);
                this->warn(
                    "{}: Failed to copy documentation from {} '{}' (no documentation available).\n"
                    "    No documentation available.\n"
                    "        {}:{}\n"
                    "        Note: No documentation available for '{}'.",
                    corpus_.Corpus::qualifiedName(*current_context_),
                    toString(res.Kind),
                    copied->string,
                    resPrimaryLoc->FullPath,
                    resPrimaryLoc->LineNumber,
                    corpus_.Corpus::qualifiedName(res));
            }
            continue;
        }

        // Copy brief and details
        bool const copyBrief = copied->parts == doc::Parts::all || copied->parts == doc::Parts::brief;
        bool const copyDetails = copied->parts == doc::Parts::all || copied->parts == doc::Parts::description;
        Javadoc const& src = *res.javadoc;
        if (copyBrief && !javadoc.brief)
        {
            javadoc.brief = src.brief;
        }
        if (copyDetails)
        {
            // Copy detail blocks
            if (!src.blocks.empty())
            {
                blockIt = javadoc.blocks.insert(blockIt, src.blocks.begin(), src.blocks.end());
                blockIt = std::next(blockIt, src.blocks.size());
            }
            // Copy returns only if destination is empty
            if (javadoc.returns.empty())
            {
                javadoc.returns.insert(
                    javadoc.returns.end(),
                    src.returns.begin(),
                    src.returns.end());
            }
            // Copy only params that don't exist at the destination
            // documentation but that do exist in the destination
            // function parameters declaration.
            if (current_context_->isFunction())
            {
                auto const& FI = dynamic_cast<FunctionInfo const&>(*current_context_);
                for (auto const& srcParam: src.params)
                {
                    if (std::ranges::find_if(javadoc.params,
                        [&srcParam](doc::Param const& destParam)
                        {
                            return srcParam.name == destParam.name;
                        }) != javadoc.params.end())
                    {
                        // param already exists at the destination,
                        // so the user attributed a new meaning to it
                        continue;
                    }
                    if (std::ranges::find_if(FI.Params,
                        [&srcParam](Param const& destParam)
                        {
                            return srcParam.name == *destParam.Name;
                        }) == FI.Params.end())
                    {
                        // param does not exist in the destination function
                        // so it would be an error there
                        continue;
                    }
                    // Push the new param
                    javadoc.params.push_back(srcParam);
                }
            }
            // Copy only tparams that don't exist at the destination
            // documentation but that do exist in the destination
            // template parameters.
            TemplateInfo const* destTemplate = visit(
                *current_context_,
                [](auto& I) -> TemplateInfo const*
                {
                    if constexpr (requires { I.Template; })
                    {
                        if (I.Template)
                        {
                            return &*I.Template;
                        }
                    }
                    return nullptr;
                });
            if (destTemplate)
            {
                for (auto const& srcTParam: src.tparams)
                {
                    if (std::ranges::find_if(javadoc.tparams,
                        [&srcTParam](doc::TParam const& destTParam)
                        {
                            return srcTParam.name == destTParam.name;
                        }) != javadoc.tparams.end())
                    {
                        // tparam already exists at the destination,
                        // so the user attributed a new meaning to it
                        continue;
                    }
                    if (std::ranges::find_if(destTemplate->Params,
                        [&srcTParam](Polymorphic<TParam> const& destTParam)
                        {
                            return srcTParam.name == destTParam->Name;
                        }) == destTemplate->Params.end())
                    {
                        // TParam does not exist in the destination definition
                        // so it would be an error there
                        continue;
                    }
                    // Push the new param
                    javadoc.tparams.push_back(srcTParam);
                }
            }
            // exceptions
            if (javadoc.exceptions.empty())
            {
                bool const isNoExcept =
                    current_context_->isFunction() ?
                    dynamic_cast<FunctionInfo const&>(*current_context_).Noexcept.Kind == NoexceptKind::False :
                    false;
                if (!isNoExcept)
                {
                    javadoc.exceptions.insert(
                        javadoc.exceptions.end(),
                        src.exceptions.begin(),
                        src.exceptions.end());
                }
            }
            // sees
            if (javadoc.sees.empty())
            {
                javadoc.sees.insert(
                    javadoc.sees.end(),
                    src.sees.begin(),
                    src.sees.end());
            }
            // preconditions
            if (javadoc.preconditions.empty())
            {
                javadoc.preconditions.insert(
                    javadoc.preconditions.end(),
                    src.preconditions.begin(),
                    src.preconditions.end());
            }
            // postconditions
            if (javadoc.postconditions.empty())
            {
                javadoc.postconditions.insert(
                    javadoc.postconditions.end(),
                    src.postconditions.begin(),
                    src.postconditions.end());
            }
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
        ++it;
    }
}

void
JavadocFinalizer::
trimBlocks(Javadoc& javadoc)
{
    trimBlocks(javadoc.blocks);
    if (javadoc.brief)
    {
        trimBlock(*javadoc.brief);
    }
    trimBlocks(javadoc.returns);
    trimBlocks(javadoc.params);
    trimBlocks(javadoc.tparams);
    trimBlocks(javadoc.exceptions);
    trimBlocks(javadoc.sees);
    trimBlocks(javadoc.preconditions);
    trimBlocks(javadoc.postconditions);
}

void
JavadocFinalizer::
trimBlocks(std::vector<Polymorphic<doc::Block>>& blocks)
{
    for (auto& block: blocks)
    {
        bool const isVerbatim = block->Kind == doc::NodeKind::code;
        MRDOCS_CHECK_OR_CONTINUE(!isVerbatim);
        trimBlock(*block);
    }
}

void
JavadocFinalizer::
trimBlock(doc::Block& block)
{
    if (block.Kind == doc::NodeKind::unordered_list)
    {
        auto& ul = dynamic_cast<doc::UnorderedList&>(block);
        trimBlocks(ul.items);
        return;
    }

    MRDOCS_CHECK_OR(!block.children.empty());

    // Helper functions
    static constexpr std::string_view whitespace_chars = " \t\n\v\f\r";
    auto endsWithSpace = [](std::string_view const str) {
        return endsWithOneOf(str, whitespace_chars);
    };
    auto startsWithSpace = [](std::string_view const str) {
        return startsWithOneOf(str, whitespace_chars);
    };

    // The first children are ltrimmed as one
    while (!block.children.empty())
    {
        auto& first = block.children.front()->string;
        if (startsWithSpace(first))
        {
            first = ltrim(first);
        }
        if (first.empty())
        {
            // "pop_front"
            block.children.erase(block.children.begin());
        }
        else
        {
            break;
        }
    }

    // The last children are rtrimmed as one
    while (!block.children.empty())
    {
        auto& last = block.children.back()->string;
        if (endsWithSpace(last))
        {
            last = rtrim(last);
        }
        if (last.empty())
        {
            block.children.pop_back();
        }
        else
        {
            break;
        }
    }

    // Like in HTML, multiple whitespaces (spaces, tabs, and newlines)
    // between and within child nodes are collapsed into a single space.
    for (
        auto it = block.children.begin() + 1;
        it != block.children.end();
        ++it)
    {
        auto& child = *it;
        auto& prev = *std::prev(it);
        if (endsWithSpace(prev->string) && startsWithSpace(child->string))
        {
            // The first visible space character is maintained.
            // All others are removed.
            prev->string = rtrim(prev->string);
            prev->string.push_back(' ');
            child->string = ltrim(child->string);
        }
    }

    // Like in HTML, multiple whitespaces (spaces, tabs, and newlines)
    // within child nodes are collapsed into a single space.
    for (auto& child: block.children)
    {
        auto& str = child->string;
        for (std::size_t i = 0; i < str.size();)
        {
            if (contains(whitespace_chars, str[i]))
            {
                std::size_t const runStart = i;
                std::size_t runEnd = i + 1;
                while (
                    runEnd < str.size() &&
                    contains(whitespace_chars, str[runEnd]))
                {
                    ++runEnd;
                }
                if (runEnd > runStart + 1)
                {
                    std::size_t const runSize = runEnd - runStart;
                    str.erase(runStart + 1, runSize - 1);
                    str[runStart] = ' ';
                }
                i = runEnd;
            }
            else
            {
                ++i;
            }
        }
    }
}

void
JavadocFinalizer::
removeTempTextNodes(Javadoc& javadoc)
{
    removeTempTextNodes(javadoc.blocks);
    if (javadoc.brief)
    {
        removeTempTextNodes(*javadoc.brief);
    }
    removeTempTextNodes(javadoc.returns);
    removeTempTextNodes(javadoc.params);
    removeTempTextNodes(javadoc.tparams);
    removeTempTextNodes(javadoc.exceptions);
    removeTempTextNodes(javadoc.sees);
    removeTempTextNodes(javadoc.preconditions);
    removeTempTextNodes(javadoc.postconditions);
}

void
JavadocFinalizer::
removeTempTextNodes(std::vector<Polymorphic<doc::Block>>& blocks)
{
    for (auto& block: blocks)
    {
        removeTempTextNodes(*block);
    }
    // Erase all blocks of zero elements
    std::erase_if(blocks, [](Polymorphic<doc::Block> const& block) {
        if (block->Kind == doc::NodeKind::unordered_list)
        {
            return get<doc::UnorderedList>(block).items.empty();
        }
        if (block->Kind == doc::NodeKind::heading)
        {
            return get<doc::Heading>(block).string.empty();
        }
        return block->children.empty();
    });
}

void
JavadocFinalizer::
removeTempTextNodes(doc::Block& block)
{
    std::erase_if(block.children, [](Polymorphic<doc::Text> const& child) {
        return is_one_of(
            child->Kind,
            { doc::NodeKind::copied });
    });
}

void
JavadocFinalizer::
unindentCodeBlocks(Javadoc& javadoc)
{
    unindentCodeBlocks(javadoc.blocks);
    if (javadoc.brief)
    {
        unindentCodeBlocks(*javadoc.brief);
    }
    unindentCodeBlocks(javadoc.returns);
    unindentCodeBlocks(javadoc.params);
    unindentCodeBlocks(javadoc.tparams);
    unindentCodeBlocks(javadoc.exceptions);
    unindentCodeBlocks(javadoc.sees);
    unindentCodeBlocks(javadoc.preconditions);
    unindentCodeBlocks(javadoc.postconditions);
}

void
JavadocFinalizer::
unindentCodeBlocks(std::vector<Polymorphic<doc::Block>>& blocks)
{
    for (auto& block: blocks)
    {
        if (block->Kind == doc::NodeKind::code)
        {
            unindentCodeBlocks(*block);
        }
    }
}

void
JavadocFinalizer::
unindentCodeBlocks(doc::Block& block)
{
    MRDOCS_CHECK_OR(block.Kind == doc::NodeKind::code);
    MRDOCS_CHECK_OR(!block.children.empty());

    // Determine the left margin
    std::size_t leftMargin = std::numeric_limits<std::size_t>::max();
    for (auto& pText: block.children)
    {
        auto& text = dynamic_cast<doc::Text&>(*pText);
        if (text.string.empty())
        {
            continue;
        }
        std::size_t const margin = text.string.find_first_not_of(" \t");
        if (margin == std::string::npos)
        {
            continue;
        }
        leftMargin = std::min(leftMargin, margin);
    }

    MRDOCS_CHECK_OR(leftMargin != std::numeric_limits<std::size_t>::max());

    // Remove the left margin
    for (auto& pText: block.children)
    {
        auto& text = dynamic_cast<doc::Text&>(*pText);
        if (text.string.size() < leftMargin)
        {
            continue;
        }
        text.string = text.string.substr(leftMargin);
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
    finalize(innerTypePtr(type));

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

namespace {
template <class Range>
bool
populateOverloadsBriefIfAllSameBrief(OverloadsInfo& I, Range&& functionsWithBrief)
{
    auto first = *functionsWithBrief.begin();
    doc::Brief const& firstBrief = *first.javadoc->brief;
    if (auto otherFunctions = std::views::drop(functionsWithBrief, 1);
        std::ranges::all_of(otherFunctions, [&](FunctionInfo const& otherFunction)
        {
            doc::Brief const& otherBrief = *otherFunction.javadoc->brief;
            return otherBrief == firstBrief;
        }))
    {
        I.javadoc->brief = firstBrief;
        return true;
    }
    return false;
}

void
setBriefString(std::optional<doc::Brief>& brief, std::string_view str) {
    brief.emplace();
    brief->children.emplace_back(MakePolymorphic<doc::Text>(std::string(str)));
}

bool
populateOverloadsFromClass(OverloadsInfo& I)
{
    switch (I.Class)
    {
        case FunctionClass::Normal:
            return false;
        case FunctionClass::Constructor:
        {
            setBriefString(I.javadoc->brief, "Constructors");
            return true;
        }
        case FunctionClass::Conversion:
        {
            setBriefString(I.javadoc->brief, "Conversion operators");
            return true;
        }
        case FunctionClass::Destructor:
        default:
        MRDOCS_UNREACHABLE();
    }
}

template <class Range>
bool
populateOverloadsFromOperator(OverloadsInfo& I, Range&& functions)
{
    if (I.OverloadedOperator == OperatorKind::None)
    {
        return false;
    }
    // An array of pairs describing the operator kind and the
    // default brief string for that operator kind.
    struct OperatorBrief {
        OperatorKind kind = OperatorKind::None;
        std::string_view brief;
        std::string_view binaryBrief;
        constexpr
        OperatorBrief(
            OperatorKind kind,
            std::string_view brief,
            std::string_view binaryBrief = "")
            : kind(kind)
            , brief(brief)
            , binaryBrief(binaryBrief) {}
    };
    static constexpr OperatorBrief operatorBriefs[] = {
        {OperatorKind::Equal, "Assignment operators"},
        {OperatorKind::Star, "Dereference operators", "Multiplication operators"},
        {OperatorKind::Arrow, "Member access operators"},
        {OperatorKind::Exclaim, "Negation operators"},
        {OperatorKind::EqualEqual, "Equality operators"},
        {OperatorKind::ExclaimEqual, "Inequality operators"},
        {OperatorKind::Less, "Less-than operators"},
        {OperatorKind::LessEqual, "Less-than-or-equal operators"},
        {OperatorKind::Greater, "Greater-than operators"},
        {OperatorKind::GreaterEqual, "Greater-than-or-equal operators"},
        {OperatorKind::Spaceship, "Three-way comparison operators"},
        {OperatorKind::AmpAmp, "Conjunction operators"},
        {OperatorKind::PipePipe, "Disjunction operators"},
        {OperatorKind::PlusPlus, "Increment operators"},
        {OperatorKind::MinusMinus, "Decrement operators"},
        {OperatorKind::Comma, "Comma operators"},
        {OperatorKind::ArrowStar, "Pointer-to-member operators"},
        {OperatorKind::Call, "Function call operators"},
        {OperatorKind::Subscript, "Subscript operators"},
        {OperatorKind::Conditional, "Ternary operators"},
        {OperatorKind::Coawait, "Coawait operators"},
        {OperatorKind::New, "New operators"},
        {OperatorKind::Delete, "Delete operators"},
        {OperatorKind::ArrayNew, "New array operators"},
        {OperatorKind::ArrayDelete, "Delete array operators"},
        {OperatorKind::Plus, "Unary plus operators", "Addition operators"},
        {OperatorKind::Minus, "Unary minus operators", "Subtraction operators"},
        {OperatorKind::Slash, "Division operators"},
        {OperatorKind::Percent, "Modulus operators"},
        {OperatorKind::Pipe, "Bitwise disjunction operators"},
        {OperatorKind::Caret, "Bitwise exclusive-or operators"},
        {OperatorKind::Tilde, "Bitwise negation operators"},
        {OperatorKind::PlusEqual, "Addition assignment operators"},
        {OperatorKind::MinusEqual, "Subtraction assignment operators"},
        {OperatorKind::StarEqual, "Multiplication assignment operators"},
        {OperatorKind::SlashEqual, "Division assignment operators"},
        {OperatorKind::PercentEqual, "Modulus assignment operators"},
        {OperatorKind::Amp, "Address-of operators", "Bitwise conjunction operators"},
        {OperatorKind::AmpEqual, "Bitwise conjunction assignment operators"},
        {OperatorKind::PipeEqual, "Bitwise disjunction assignment operators"},
        {OperatorKind::CaretEqual, "Bitwise exclusive-or assignment operators"},
        {OperatorKind::LessLess, "Left shift operators"},
        {OperatorKind::GreaterGreater, "Right shift operators"},
        {OperatorKind::LessLessEqual, "Left shift assignment operators"},
        {OperatorKind::GreaterGreaterEqual, "Right shift assignment operators"}
    };
    for (auto const& [kind, brief, binaryBrief]: operatorBriefs)
    {
        MRDOCS_CHECK_OR_CONTINUE(I.OverloadedOperator == kind);

        // The name for operator<< depends on the parameter types
        if (kind == OperatorKind::LessLess)
        {
            // Check if all functions are Stream Operators:
            // 1) Non-member function
            // 2) First param is mutable reference
            // 3) Return type is mutable reference of same type as first param
            if (std::ranges::all_of(functions,
            [&](FunctionInfo const& function)
                {
                    MRDOCS_CHECK_OR(!function.IsRecordMethod, false);
                    MRDOCS_CHECK_OR(function.Params.size() == 2, false);
                    // Check first param is mutable left reference
                    auto& firstParam = function.Params[0];
                    MRDOCS_CHECK_OR(firstParam, false);
                    auto& firstQualType = firstParam.Type;
                    MRDOCS_CHECK_OR(firstQualType, false);
                    MRDOCS_CHECK_OR(firstQualType->isLValueReference(), false);
                    auto& firstNamedType = get<LValueReferenceTypeInfo const&>(firstQualType).PointeeType;
                    MRDOCS_CHECK_OR(firstNamedType, false);
                    MRDOCS_CHECK_OR(firstNamedType->isNamed(), false);
                    // Check return type
                    return firstQualType == function.ReturnType;
                }))
            {
                setBriefString(I.javadoc->brief, "Stream insertion operators");
            }
            else
            {
                // Regular brief as more generic left shift operator otherwise
                setBriefString(I.javadoc->brief, brief);
            }
            return true;
        }

        if (binaryBrief.empty())
        {
            setBriefString(I.javadoc->brief, brief);
            return true;
        }

        if (std::ranges::all_of(functions,
            [&](FunctionInfo const& function)
            {
                return (function.Params.size() + function.IsRecordMethod) == 2;
            }))
        {
            setBriefString(I.javadoc->brief, binaryBrief);
            return true;
        }

        if (std::ranges::all_of(functions,
            [&](FunctionInfo const& function)
            {
                return (function.Params.size() + function.IsRecordMethod) == 1;
            }))
        {
            setBriefString(I.javadoc->brief, brief);
            return true;
        }
        return false;
    }
    return false;
}

bool
populateOverloadsFromFunctionName(OverloadsInfo& I)
{
    std::string name = I.Name;
    if (name.empty() &&
        I.OverloadedOperator != OperatorKind::None)
    {
        name = getOperatorName(I.OverloadedOperator, true);
    }
    if (name.empty())
    {
        return false;
    }
    I.javadoc->brief.emplace();
    I.javadoc->brief->children.emplace_back(MakePolymorphic<doc::Text, doc::Styled>(std::string(name), doc::Style::mono));
    I.javadoc->brief->children.emplace_back(MakePolymorphic<doc::Text>(std::string(" overloads")));
    return true;
}

template <class Range>
void
populateOverloadsBrief(OverloadsInfo& I, Range&& functions)
{
    auto functionsWithBrief = std::views::filter(functions,
        [](FunctionInfo const& function)
        {
            return
                function.javadoc &&
                function.javadoc->brief &&
                !function.javadoc->brief->empty();
        });
    if (std::ranges::empty(functionsWithBrief))
    {
        return;
    }
    MRDOCS_CHECK_OR(!populateOverloadsBriefIfAllSameBrief(I, functionsWithBrief));
    MRDOCS_CHECK_OR(!populateOverloadsFromClass(I));
    MRDOCS_CHECK_OR(!populateOverloadsFromOperator(I, functions));
    MRDOCS_CHECK_OR(!populateOverloadsFromFunctionName(I));
}

template <class Range>
void
populateOverloadsReturns(OverloadsInfo& I, Range&& functions) {
    auto functionReturns = functions |
        std::views::filter([](FunctionInfo const& function)
            {
                return function.javadoc && !function.javadoc->returns.empty();
            }) |
        std::views::transform([](FunctionInfo const& function)
            {
                return function.javadoc->returns;
            }) |
        std::views::join;
    for (doc::Returns const& functionReturn: functionReturns)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->returns,
            [&functionReturn](doc::Returns const& overloadReturns)
            {
                return overloadReturns == functionReturn;
            });
        if (sameIt == I.javadoc->returns.end())
        {
            I.javadoc->returns.push_back(functionReturn);
        }
    }
}

template <class Range>
void
populateOverloadsParams(OverloadsInfo& I, Range& functions) {
    auto functionParams = functions |
        std::views::filter([](FunctionInfo const& function)
            {
                return function.javadoc && !function.javadoc->params.empty();
            }) |
        std::views::transform([](FunctionInfo const& function)
            {
                return function.javadoc->params;
            }) |
        std::views::join;
    for (doc::Param const& functionParam: functionParams)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->params,
            [&functionParam](doc::Param const& overloadParam)
            {
                return overloadParam.name == functionParam.name;
            });
        if (sameIt == I.javadoc->params.end())
        {
            I.javadoc->params.push_back(functionParam);
        }
    }
}

template <class Range>
void
populateOverloadsTParams(OverloadsInfo& I, Range& functions) {
    auto functionTParams = functions |
        std::views::filter([](FunctionInfo const& function)
            {
                return function.javadoc && !function.javadoc->tparams.empty();
            }) |
        std::views::transform([](FunctionInfo const& function)
            {
                return function.javadoc->tparams;
            }) |
        std::views::join;
    for (doc::TParam const& functionTParam: functionTParams)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->tparams,
            [&functionTParam](doc::TParam const& overloadTParam)
            {
                return overloadTParam.name == functionTParam.name;
            });
        if (sameIt == I.javadoc->tparams.end())
        {
            I.javadoc->tparams.push_back(functionTParam);
        }
    }
}

template <class Range>
void
populateOverloadsExceptions(OverloadsInfo& I, Range& functions) {
    auto functionExceptions = functions |
        std::views::filter([](FunctionInfo const& function)
            {
                return function.javadoc && !function.javadoc->exceptions.empty();
            }) |
        std::views::transform([](FunctionInfo const& function)
            {
                return function.javadoc->exceptions;
            }) |
        std::views::join;
    for (doc::Throws const& functionException: functionExceptions)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->exceptions,
            [&functionException](doc::Throws const& overloadException)
            {
                return overloadException.exception.string == functionException.exception.string;
            });
        if (sameIt == I.javadoc->exceptions.end())
        {
            I.javadoc->exceptions.push_back(functionException);
        }
    }
}

template <class Range>
void
populateOverloadsSees(OverloadsInfo& I, Range& functions) {
    auto functionSees = functions |
        std::views::filter([](FunctionInfo const& function)
            {
                return function.javadoc && !function.javadoc->sees.empty();
            }) |
        std::views::transform([](FunctionInfo const& function)
            {
                return function.javadoc->sees;
            }) |
        std::views::join;
    for (doc::See const& functionSee: functionSees)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->sees,
            [&functionSee](doc::See const& overloadSee)
            {
                return overloadSee.children == functionSee.children;
            });
        if (sameIt == I.javadoc->sees.end())
        {
            I.javadoc->sees.push_back(functionSee);
        }
    }
}

template <class Range>
void
populateOverloadsPreconditions(OverloadsInfo& I, Range& functions) {
    auto functionsPres = functions |
        std::views::filter([](FunctionInfo const& function)
            {
                return function.javadoc && !function.javadoc->preconditions.empty();
            }) |
        std::views::transform([](FunctionInfo const& function)
            {
                return function.javadoc->preconditions;
            }) |
        std::views::join;
    for (doc::Precondition const& functionPre: functionsPres)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->preconditions,
            [&functionPre](doc::Precondition const& overloadPre)
            {
                return overloadPre.children == functionPre.children;
            });
        if (sameIt == I.javadoc->preconditions.end())
        {
            I.javadoc->preconditions.push_back(functionPre);
        }
    }
}

template <class Range>
void
populateOverloadsPostconditions(OverloadsInfo& I, Range& functions) {
    auto functionsPosts = functions |
        std::views::filter([](FunctionInfo const& function)
            {
                return function.javadoc && !function.javadoc->postconditions.empty();
            }) |
        std::views::transform([](FunctionInfo const& function)
            {
                return function.javadoc->postconditions;
            }) |
        std::views::join;
    for (doc::Postcondition const& functionPost: functionsPosts)
    {
        auto sameIt = std::ranges::find_if(
            I.javadoc->postconditions,
            [&functionPost](doc::Postcondition const& overloadPost)
            {
                return overloadPost.children == functionPost.children;
            });
        if (sameIt == I.javadoc->postconditions.end())
        {
            I.javadoc->postconditions.push_back(functionPost);
        }
    }
}
} // (anon)


void
JavadocFinalizer::
populateOverloadJavadocs(OverloadsInfo& I)
{
    // Create a view all Info members of I
    auto functions =
        I.Members |
        std::views::transform([&](SymbolID const& id)
            {
                return corpus_.find(id);
            }) |
        std::views::filter([](Info const* infoPtr)
            {
                return infoPtr != nullptr && infoPtr->isFunction();
            }) |
        std::views::transform([](Info const* infoPtr) -> FunctionInfo const&
            {
                return *dynamic_cast<FunctionInfo const*>(infoPtr);
            });

    // Ensure all the members are initialized
    for (FunctionInfo const& function: functions)
    {
        if (!finalized_.contains(&function))
        {
            operator()(const_cast<FunctionInfo&>(function));
        }
    }

    I.javadoc.emplace();
    // blocks: we do not copy javadoc detail blocks because
    // it's impossible to guarantee that the details for
    // any of the functions make sense for all overloads
    populateOverloadsBrief(I, functions);
    populateOverloadsReturns(I, functions);
    populateOverloadsParams(I, functions);
    populateOverloadsTParams(I, functions);
    populateOverloadsExceptions(I, functions);
    populateOverloadsSees(I, functions);
    populateOverloadsPreconditions(I, functions);
    populateOverloadsPostconditions(I, functions);
}


void
JavadocFinalizer::
emitWarnings()
{
    MRDOCS_CHECK_OR(corpus_.config->warnings);
    warnUndocumented();
    warnDocErrors();
    warnNoParamDocs();
    warnUndocEnumValues();
    warnUnnamedParams();

    // Print to the console
    auto const level = !corpus_.config->warnAsError ? report::Level::warn : report::Level::error;
    for (auto const& [loc, msgs] : warnings_)
    {
        std::string locWarning = fmt::format(
            "{}:{}\n",
            loc.FullPath,
            loc.LineNumber);
        int i = 1;
        for (auto const& msg : msgs)
        {
            locWarning += fmt::format("    {}) {}\n", i++, msg);
        }
        report::log(level, locWarning);
    }
}

void
JavadocFinalizer::
warnUndocumented()
{
    MRDOCS_CHECK_OR(corpus_.config->warnIfUndocumented);
    for (auto& undocI : corpus_.undocumented_)
    {
        if (Info const* I = corpus_.find(undocI.id))
        {
            MRDOCS_CHECK_OR(
                !I->javadoc || I->Extraction == ExtractionMode::Regular);
        }
        bool const prefer_definition = undocI.kind == InfoKind::Record
                                 || undocI.kind == InfoKind::Enum;
        this->warn(
            *getPrimaryLocation(undocI, prefer_definition),
            "{}: Symbol is undocumented", undocI.name);
    }
    corpus_.undocumented_.clear();
}

void
JavadocFinalizer::
warnDocErrors()
{
    MRDOCS_CHECK_OR(corpus_.config->warnIfDocError);
    for (auto const& I : corpus_.info_)
    {
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(I->isFunction());
        warnParamErrors(dynamic_cast<FunctionInfo const&>(*I));
    }
}

namespace {
/* Get a list of all parameter names in javadoc

    The javadoc parameter names can contain a single parameter or
    a list of parameters separated by commas. This function
    returns a list of all parameter names in the javadoc.
 */
SmallVector<std::string_view, 32>
getJavadocParamNames(Javadoc const& javadoc)
{
    SmallVector<std::string_view, 32> result;
    for (auto const& javadocParam: javadoc.params)
    {
        auto const& paramNamesStr = javadocParam.name;
        for (auto paramNames = std::views::split(paramNamesStr, ',');
             auto const& paramName: paramNames)
        {
            result.push_back(trim(std::string_view(paramName.begin(), paramName.end())));
        }
    }
    return result;
}

}

void
JavadocFinalizer::
warnParamErrors(FunctionInfo const& I)
{
    MRDOCS_CHECK_OR(I.javadoc);

    // Check for duplicate javadoc parameters
    auto javadocParamNames = getJavadocParamNames(*I.javadoc);
    std::ranges::sort(javadocParamNames);
    auto [firstDup, lastUnique] = std::ranges::unique(javadocParamNames);
    auto duplicateParamNames = std::ranges::subrange(firstDup, lastUnique);
    auto [firstDupDup, _] = std::ranges::unique(duplicateParamNames);
    for (auto const uniqueDuplicateParamNames = std::ranges::subrange(firstDup, firstDupDup);
         std::string_view duplicateParamName: uniqueDuplicateParamNames)
    {
        this->warn(
            *getPrimaryLocation(I),
            "{}: Duplicate parameter documentation for '{}'",
            corpus_.Corpus::qualifiedName(I),
            duplicateParamName);
    }
    javadocParamNames.erase(lastUnique, javadocParamNames.end());

    // Check for documented parameters that don't exist in the function
    auto paramNames =
        std::views::transform(I.Params, &Param::Name) |
        std::views::filter([](Optional<std::string> const& name) { return static_cast<bool>(name); }) |
        std::views::transform([](Optional<std::string> const& name) -> std::string_view { return *name; });
    for (std::string_view javadocParamName: javadocParamNames)
    {
        if (std::ranges::find(paramNames, javadocParamName) == paramNames.end())
        {
            this->warn(
                *getPrimaryLocation(I),
                "{}: Documented parameter '{}' does not exist",
                corpus_.Corpus::qualifiedName(I),
                javadocParamName);
        }
    }

}

void
JavadocFinalizer::
warnNoParamDocs()
{
    MRDOCS_CHECK_OR(corpus_.config->warnNoParamdoc);
    for (auto const& I : corpus_.info_)
    {
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(I->isFunction());
        MRDOCS_CHECK_OR_CONTINUE(I->javadoc);
        warnNoParamDocs(dynamic_cast<FunctionInfo const&>(*I));
    }
}

void
JavadocFinalizer::
warnNoParamDocs(FunctionInfo const& I)
{
    MRDOCS_CHECK_OR(!I.IsDeleted);
    // Check for function parameters that are not documented in javadoc
    auto javadocParamNames = getJavadocParamNames(*I.javadoc);
    auto paramNames =
        std::views::transform(I.Params, &Param::Name) |
        std::views::transform([](Optional<std::string> const& name) -> std::string_view { return *name; }) |
        std::views::filter([](std::string_view const& name) { return !name.empty(); });
    for (auto const& paramName: paramNames)
    {
        if (std::ranges::find(javadocParamNames, paramName) == javadocParamNames.end())
        {
            this->warn(
                *getPrimaryLocation(I),
                "{}: Missing documentation for parameter '{}'",
                corpus_.Corpus::qualifiedName(I),
                paramName);
        }
    }

    // Check for undocumented return type
    if (I.javadoc->returns.empty() && I.ReturnType)
    {
        auto isVoid = [](TypeInfo const& returnType) -> bool
        {
            if (returnType.isNamed())
            {
                auto const& namedReturnType = dynamic_cast<NamedTypeInfo const&>(returnType);
                return namedReturnType.Name->Name == "void";
            }
            return false;
        };
        if (!isVoid(*I.ReturnType))
        {
            this->warn(
                *getPrimaryLocation(I),
                "{}: Missing documentation for return type",
                corpus_.Corpus::qualifiedName(I));
        }
    }
}

void
JavadocFinalizer::
warnUndocEnumValues()
{
    MRDOCS_CHECK_OR(corpus_.config->warnIfUndocEnumVal);
    for (auto const& I : corpus_.info_)
    {
        MRDOCS_CHECK_OR_CONTINUE(I->isEnumConstant());
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(!I->javadoc);
        this->warn(
            *getPrimaryLocation(*I),
            "{}: Missing documentation for enum value",
            corpus_.Corpus::qualifiedName(*I));
    }
}

void
JavadocFinalizer::
warnUnnamedParams()
{
    MRDOCS_CHECK_OR(corpus_.config->warnUnnamedParam);
    for (auto const& I : corpus_.info_)
    {
        MRDOCS_CHECK_OR_CONTINUE(I->isFunction());
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(I->javadoc);
        warnUnnamedParams(dynamic_cast<FunctionInfo const&>(*I));
    }
}

void
JavadocFinalizer::
warnUnnamedParams(FunctionInfo const& I)
{
    auto orderSuffix = [](std::size_t const i) -> std::string
    {
        if (i == 0)
        {
            return "st";
        }
        if (i == 1)
        {
            return "nd";
        }
        if (i == 2)
        {
            return "rd";
        }
        return "th";
    };

    for (std::size_t i = 0; i < I.Params.size(); ++i)
    {
        if (!I.Params[i].Name)
        {
            this->warn(
                *getPrimaryLocation(I),
                "{}: {}{} parameter is unnamed",
                corpus_.Corpus::qualifiedName(I),
                i + 1,
                orderSuffix(i));
        }
    }
}

} // clang::mrdocs
