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

#define INFO(T) template void JavadocFinalizer::operator()<T## Info>(T## Info&);
#include <mrdocs/Metadata/InfoNodesPascal.inc>

void
JavadocFinalizer::
finalize(doc::Reference& ref, bool const emitWarning)
{
    if (Expected<Info const*> res
        = corpus_.lookup(current_context_->id, ref.string))
    {
        Info const* resPtr = *res;
        MRDOCS_ASSERT(resPtr);
        ref.id = resPtr->id;
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
            res.error().reason());
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
        Expected<Info const*> res = corpus_.lookup(current_context_->id, copied->string);
        if (!res || !*res)
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
                    res.error().reason());
            }
            continue;
        }

        // Ensure the source node is finalized
        Info const* resPtr = *res;
        MRDOCS_ASSERT(resPtr);
        if (!finalized_.contains(resPtr))
        {
            operator()(const_cast<Info&>(*resPtr));
        }
        if (!resPtr->javadoc)
        {
            if (corpus_.config->warnings &&
                corpus_.config->warnBrokenRef &&
                !refWarned_.contains({copied->string, current_context_->Name}))
            {
                auto resPrimaryLoc = getPrimaryLocation(*resPtr);
                this->warn(
                    "{}: Failed to copy documentation from '{}' (no documentation available).\n"
                    "    No documentation available.\n"
                    "        {}:{}\n"
                    "        Note: No documentation available for '{}'.",
                    corpus_.Corpus::qualifiedName(*current_context_),
                    copied->string,
                    resPrimaryLoc->FullPath,
                    resPrimaryLoc->LineNumber,
                    corpus_.Corpus::qualifiedName(*resPtr));
            }
            continue;
        }

        // Copy brief and details
        bool const copyBrief = copied->parts == doc::Parts::all || copied->parts == doc::Parts::brief;
        bool const copyDetails = copied->parts == doc::Parts::all || copied->parts == doc::Parts::description;
        Javadoc const& src = *resPtr->javadoc;
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
                            return srcParam.name == destParam.Name;
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

void
JavadocFinalizer::
emitWarnings()
{
    MRDOCS_CHECK_OR(corpus_.config->warnings);
    warnUndocumented();
    warnDocErrors();
    warnNoParamDocs();
    warnUndocEnumValues();

    // Print to the console
    auto const level = !corpus_.config->warnAsError ? report::Level::warn : report::Level::error;
    for (auto const& [loc, msgs] : warnings_)
    {
        std::string locWarning = fmt::format(
            "{}:{}\n",
            loc.FullPath,
            loc.LineNumber);
        for (auto const& msg : msgs)
        {
            locWarning += fmt::format("    {}\n", msg);
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
        std::views::filter([](std::string_view const& name) { return !name.empty(); });
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

} // clang::mrdocs
