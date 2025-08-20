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
#include "Javadoc/Function.hpp"
#include "Javadoc/Overloads.hpp"
#include <algorithm>
#include <format>
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/ScopeExit.hpp>

namespace clang::mrdocs {

void
JavadocFinalizer::
build()
{
    // This function finalizes groups of javadoc components in
    // different loops. This allows us to resolve references
    // that are only related to that component group without
    // creating circular dependencies.

    // Finalize briefs:
    // We do it first because all other steps require accessing
    // the brief of other functions, these often need to be resolved
    // with @copybrief or auto-brief, and we need to ensure that
    // there are no circular dependencies for other metadata.
    for (auto& infoPtr : corpus_.info_)
    {
        MRDOCS_ASSERT(infoPtr);
        Info& I = *infoPtr;
        MRDOCS_CHECK_OR_CONTINUE(I.Extraction != ExtractionMode::Dependency);
        finalizeBrief(I);
    }

    // Finalize metadata:
    // A @copydetails command also implies we should copy
    // other metadata from the referenced symbol.
    // We do it now because we need the complete metadata
    // for all objects to generate javadoc for overloads.
    // For instance, overloads cannot aggregate function
    // parameters as if the parameters are not resolved.
    for (auto& infoPtr : corpus_.info_)
    {
        MRDOCS_ASSERT(infoPtr);
        Info& I = *infoPtr;
        MRDOCS_CHECK_OR_CONTINUE(I.Extraction != ExtractionMode::Dependency);
        finalizeMetadataCopies(I);
    }

    // Create javadoc for overloads
    // - We do it before the references because the overloads
    //   themselves can be used in the references. For instance,
    //   `@ref foo` refers to the overload set because it doesn't
    //   specify the function signature.
    if (corpus_.config->overloads)
    {
        for (auto& infoPtr : corpus_.info_)
        {
            MRDOCS_ASSERT(infoPtr);
            Info& I = *infoPtr;
            MRDOCS_CHECK_OR_CONTINUE(I.isOverloads());
            MRDOCS_CHECK_OR_CONTINUE(I.Extraction != ExtractionMode::Dependency);
            if (!I.javadoc)
            {
                I.javadoc.emplace();
            }
            populateOverloadJavadoc(infoPtr->asOverloads());
        }
    }

    // Resolve references in the javadoc
    for (auto& infoPtr : corpus_.info_)
    {
        MRDOCS_ASSERT(infoPtr);
        Info& I = *infoPtr;
        MRDOCS_CHECK_OR_CONTINUE(I.Extraction != ExtractionMode::Dependency);
        finalizeJavadoc(I);
    }

    // Populate trivial function metadata
    // - We do it after the overloads because they should not
    //   rely on metadata inherited from automatic generated javadoc
    // - We also do it after the references because some metadata
    //   might be resolved from references with @copydetails
    if (corpus_.config->autoFunctionMetadata)
    {
        for (auto& infoPtr : corpus_.info_)
        {
            MRDOCS_ASSERT(infoPtr);
            Info& I = *infoPtr;
            MRDOCS_CHECK_OR_CONTINUE(I.isFunction());
            MRDOCS_CHECK_OR_CONTINUE(I.Extraction != ExtractionMode::Dependency);
            populateFunctionJavadoc(I.asFunction());
        }
    }

    // Remove invalid references in the Info objects
    for (auto& infoPtr : corpus_.info_)
    {
        MRDOCS_ASSERT(infoPtr);
        Info& I = *infoPtr;
        visit(I, [&](auto& U) {
            finalizeInfoData(U);
        });
    }

    // - Emitting param warning require everything to be completely
    //   processed
    emitWarnings();
}

void
JavadocFinalizer::
finalizeBrief(Info& I)
{
    MRDOCS_CHECK_OR(!finalized_brief_.contains(&I));
    finalized_brief_.emplace(&I);
    ScopeExitRestore s(current_context_, &I);

    report::trace(
            "Finalizing brief for '{}'",
            corpus_.Corpus::qualifiedName(I));

    if (I.isOverloads())
    {
        // Overloads are expected not to have javadoc.
        // We'll create a javadoc for them if they don't have one.
        if (!I.javadoc)
        {
            I.javadoc.emplace();
        }
        // The brief of an overload is always empty
        auto& OI = I.asOverloads();
        for (auto const& MemberIDs = OI.Members;
             auto& memberID : MemberIDs)
        {
            Info* member = corpus_.find(memberID);
            MRDOCS_CHECK_OR_CONTINUE(member);
            finalizeBrief(*member);
        }
        auto functions = overloadFunctionsRange(OI, corpus_);
        populateOverloadsBrief(OI, functions, corpus_);
        return;
    }

    MRDOCS_CHECK_OR(I.javadoc);
    // Copy brief from other symbols if there's a @copydoc
    copyBrief(*I.javadoc);
    // Set auto brief if brief is still empty
    setAutoBrief(*I.javadoc);
}

void
JavadocFinalizer::
copyBrief(Javadoc& javadoc)
{
    MRDOCS_CHECK_OR(javadoc.brief);
    MRDOCS_CHECK_OR(!javadoc.brief->copiedFrom.empty());
    MRDOCS_CHECK_OR(javadoc.brief->children.empty());

    for (std::string const& ref: javadoc.brief->copiedFrom)
    {
        // Look for source
        auto resRef =
            corpus_.lookup(current_context_->id, ref);

        // Check if the source exists
        if (!resRef)
        {
            if (corpus_.config->warnings &&
                corpus_.config->warnBrokenRef &&
                !refWarned_.contains({ref, current_context_->Name}))
            {
                this->warn(
                    "{}: Failed to copy brief from '{}' (symbol not found)\n"
                    "    {}",
                    corpus_.Corpus::qualifiedName(*current_context_),
                    ref,
                    resRef.error().reason());
            }
            continue;
        }

        // Ensure the brief source is finalized
        Info const& res = *resRef;
        finalizeBrief(const_cast<Info&>(res));

        // Check if the source has a brief
        if (!res.javadoc ||
            !res.javadoc->brief.has_value())
        {
            if (corpus_.config->warnings &&
                corpus_.config->warnBrokenRef &&
                !refWarned_.contains({ref, current_context_->Name}))
            {
                auto resPrimaryLoc = getPrimaryLocation(res);
                this->warn(
                    "{}: Failed to copy brief from {} '{}' (no brief available).\n"
                    "    No brief available.\n"
                    "        {}:{}\n"
                    "        Note: No brief available for '{}'.",
                    corpus_.Corpus::qualifiedName(*current_context_),
                    toString(res.Kind),
                    ref,
                    resPrimaryLoc->FullPath,
                    resPrimaryLoc->LineNumber,
                    corpus_.Corpus::qualifiedName(res));
            }
            continue;
        }

        Javadoc const& src = *res.javadoc;
        javadoc.brief->children = src.brief->children;
        return;
    }
}

void
JavadocFinalizer::
setAutoBrief(Javadoc& javadoc) const
{
    MRDOCS_CHECK_OR(corpus_.config->autoBrief);
    MRDOCS_CHECK_OR(!javadoc.brief);
    MRDOCS_CHECK_OR(!javadoc.blocks.empty());

    auto isInvalidBriefText = [](Polymorphic<doc::Text> const& text) {
        return !text || text->string.empty()
               || text->Kind == doc::NodeKind::copy_details
               || isWhitespace(text->string);
    };

    for (auto it = javadoc.blocks.begin(); it != javadoc.blocks.end();)
    {
        if (auto& block = *it;
            block->Kind == doc::NodeKind::paragraph ||
            block->Kind == doc::NodeKind::details)
        {
            auto& para = dynamic_cast<doc::Paragraph&>(*block);
            if (std::ranges::all_of(para.children, isInvalidBriefText))
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

namespace {
TemplateInfo const*
getTemplateInfo(Info& I)
{
    return visit(I, [](auto& I) -> TemplateInfo const* {
        if constexpr (requires { I.Template; })
        {
            if (I.Template)
            {
                return &*I.Template;
            }
        }
        return nullptr;
    });
}
}

void
JavadocFinalizer::
finalizeMetadataCopies(Info& I)
{
    MRDOCS_CHECK_OR(!finalized_metadata_.contains(&I));
    finalized_metadata_.emplace(&I);
    ScopeExitRestore s(current_context_, &I);

    report::trace(
            "Finalizing metadata for '{}'",
            corpus_.Corpus::qualifiedName(I));

    MRDOCS_CHECK_OR(I.javadoc);
    MRDOCS_CHECK_OR(!I.javadoc->blocks.empty());
    Javadoc& destJavadoc = *I.javadoc;

    SmallVector<doc::CopyDetails, 16> copiedRefs;
    for (auto& block: destJavadoc.blocks)
    {
        MRDOCS_CHECK_OR_CONTINUE(
            block->Kind == doc::NodeKind::paragraph
            || block->Kind == doc::NodeKind::details);
        auto& para = dynamic_cast<doc::Paragraph&>(*block);
        MRDOCS_CHECK_OR_CONTINUE(!para.children.empty());

        for (auto& text: para.children)
        {
            MRDOCS_CHECK_OR_CONTINUE(text->Kind == doc::NodeKind::copy_details);
            copiedRefs.emplace_back(dynamic_cast<doc::CopyDetails&>(*text));
        }
        MRDOCS_CHECK_OR_CONTINUE(!copiedRefs.empty());
    }

    for (doc::CopyDetails const& copied: copiedRefs)
    {
        // Find element
        auto resRef = corpus_.lookup(current_context_->id, copied.string);
        if (!resRef)
        {
            if (corpus_.config->warnings &&
                corpus_.config->warnBrokenRef &&
                !refWarned_.contains({copied.string, current_context_->Name}))
            {
                this->warn(
                    "{}: Failed to copy metadata from '{}' (symbol not found)\n"
                    "    {}",
                    corpus_.Corpus::qualifiedName(*current_context_),
                    copied.string,
                    resRef.error().reason());
            }
            continue;
        }

        // Function to copy the metadata from a ranges
        // of source functions. This range might
        // contain more than one function if the
        // destination is an overload set.
        // We can't copy directly from the overload set
        // because its metadata is not created at this
        // step yet.
        auto copyInfoRangeMetadata = [&](ArrayRef<Info const*> srcInfoPtrs)
        {
            auto srcInfos = srcInfoPtrs
                            | std::views::transform(
                                [](Info const* ptr) -> Info const& {
                return *ptr;
            });

            // Ensure the source metadata is finalized
            for (auto& srcInfo: srcInfos)
            {
                auto& mutSrcInfo = const_cast<Info&>(srcInfo);
                finalizeMetadataCopies(mutSrcInfo);
            }

            // Copy returns only if destination is empty
            if (destJavadoc.returns.empty())
            {
                for (auto const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.javadoc);
                    for (doc::Returns const& returnsEl: srcInfo.javadoc->returns)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destJavadoc.returns, returnsEl));
                        destJavadoc.returns.push_back(returnsEl);
                    }
                }
            }

            // Copy only params that don't exist at the destination
            // documentation but that do exist in the destination
            // function parameters declaration.
            if (I.isFunction())
            {
                auto& destF = I.asFunction();
                for (Info const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.isFunction());
                    auto const& srcFunction = srcInfo.asFunction();
                    MRDOCS_CHECK_OR_CONTINUE(srcFunction.javadoc);
                    for (Javadoc const& srcJavadoc = *srcFunction.javadoc;
                         auto const& srcDocParam: srcJavadoc.params)
                    {
                        // check if param doc doesn't already exist
                        MRDOCS_CHECK_OR_CONTINUE(
                            std::ranges::none_of(
                                destJavadoc.params,
                                [&srcDocParam](doc::Param const& destDocParam) {
                            return srcDocParam.name == destDocParam.name;
                        }));

                        // check if param name exists in the destination function
                        MRDOCS_CHECK_OR_CONTINUE(
                            std::ranges::any_of(
                                destF.Params,
                                [&srcDocParam](Param const& destParam) {
                            return srcDocParam.name == *destParam.Name;
                        }));

                        // Push the new param ot the
                        destJavadoc.params.push_back(srcDocParam);
                    }
                }
            }

            // Copy only tparams that don't exist at the destination
            // documentation but that do exist in the destination
            // template parameters.
            if (auto const destTemplateInfo = getTemplateInfo(I))
            {
                for (Info const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.javadoc);
                    for (Javadoc const& srcJavadoc = *srcInfo.javadoc;
                         auto const& srcTParam: srcJavadoc.tparams)
                    {
                        // tparam doesn't already exist at the destination
                        MRDOCS_CHECK_OR_CONTINUE(
                            std::ranges::none_of(
                                destJavadoc.tparams,
                                [&srcTParam](doc::TParam const& destTParam) {
                            return srcTParam.name == destTParam.name;
                        }));

                        // TParam name exists in the destination definition
                        MRDOCS_CHECK_OR_CONTINUE(
                            std::ranges::any_of(
                                destTemplateInfo->Params,
                                [&srcTParam](
                                    Polymorphic<TParam> const& destTParam) {
                            return srcTParam.name == destTParam->Name;
                        }));

                        // Push the new param
                        destJavadoc.tparams.push_back(srcTParam);
                    }
                }
            }

            // Copy exceptions only if destination exceptions are empty
            // and the destination is not noexcept
            bool const destIsNoExcept =
                I.isFunction() ?
                I.asFunction().Noexcept.Kind == NoexceptKind::False :
                false;
            if (destJavadoc.exceptions.empty() &&
                !destIsNoExcept)
            {
                for (Info const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.javadoc);
                    for (doc::Throws const& exceptionsEl: srcInfo.javadoc->exceptions)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destJavadoc.exceptions, exceptionsEl));
                        destJavadoc.exceptions.push_back(exceptionsEl);
                    }
                }
            }

            // Copy sees only if destination sees are empty
            if (destJavadoc.sees.empty())
            {
                for (Info const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.javadoc);
                    for (doc::See const& seesEl: srcInfo.javadoc->sees)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destJavadoc.sees, seesEl));
                        destJavadoc.sees.push_back(seesEl);
                    }
                }
            }

            // Copy preconditions only if destination preconditions is empty
            if (destJavadoc.preconditions.empty())
            {
                for (Info const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.javadoc);
                    for (doc::Precondition const& preconditionsEl: srcInfo.javadoc->preconditions)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destJavadoc.preconditions, preconditionsEl));
                        destJavadoc.preconditions.push_back(preconditionsEl);
                    }
                }
            }

            // Copy postconditions only if destination postconditions is empty
            if (destJavadoc.postconditions.empty())
            {
                for (Info const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.javadoc);
                    for (doc::Postcondition const& postconditionsEl:
                         srcInfo.javadoc->postconditions)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(
                            destJavadoc.postconditions,
                            postconditionsEl));
                        destJavadoc.postconditions.push_back(postconditionsEl);
                    }
                }
            }
        };

        // Ensure the source metadata is finalized
        Info const& res = *resRef;
        if (!res.isOverloads())
        {
            // If it's a single element, we check the element javadoc
            if (!res.javadoc)
            {
                if (corpus_.config->warnings &&
                    corpus_.config->warnBrokenRef &&
                    !refWarned_.contains({copied.string, current_context_->Name}))
                {
                    auto resPrimaryLoc = getPrimaryLocation(res);
                    this->warn(
                        "{}: Failed to copy metadata from {} '{}' (no documentation available).\n"
                        "    No metadata available.\n"
                        "        {}:{}\n"
                        "        Note: No documentation available for '{}'.",
                        corpus_.Corpus::qualifiedName(*current_context_),
                        toString(res.Kind),
                        copied.string,
                        resPrimaryLoc->FullPath,
                        resPrimaryLoc->LineNumber,
                        corpus_.Corpus::qualifiedName(res));
                }
                continue;
            }
            SmallVector<Info const*, 1> srcInfos = { &res };
            copyInfoRangeMetadata(srcInfos);
        }
        else
        {
            auto& OI = res.asOverloads();
            SmallVector<Info const*, 16> srcInfos;
            srcInfos.reserve(OI.Members.size());
            for (auto& memberID: OI.Members)
            {
                Info* member = corpus_.find(memberID);
                MRDOCS_CHECK_OR_CONTINUE(member);
                srcInfos.push_back(member);
            }
            copyInfoRangeMetadata(srcInfos);
        }
    }
}

void
JavadocFinalizer::
populateFunctionJavadoc(FunctionInfo& I) const
{
    // For special functions (constructors, destructors, ...),
    // we create the javadoc if it does not exist because
    // we can populate all the fields from the function category.
    // For other types of functions, we'll only populate
    // the missing fields when the javadoc already exists.
    bool const isSpecial = isSpecialFunction(I);
    MRDOCS_CHECK_OR(isSpecial || I.javadoc);
    bool forceEmplaced = false;
    if (isSpecial &&
        !I.javadoc)
    {
        I.javadoc.emplace();
        forceEmplaced = true;
    }

    // Populate a missing javadoc brief
    populateFunctionBrief(I, corpus_);

    // Populate a missing javadoc returns
    populateFunctionReturns(I, corpus_);

    // Populate missing javadoc params
    populateFunctionParams(I, corpus_);

    // If we forcefully created the javadoc, we need to
    // check if the function was able to populate all the
    // fields. If not, we'll remove the javadoc.
    if (forceEmplaced)
    {
        // Check brief and returns
        if (!I.javadoc->brief)
        {
            I.javadoc.reset();
            return;
        }

        if (!is_one_of(I.Class, {
            FunctionClass::Constructor,
            FunctionClass::Destructor }) &&
            I.javadoc->returns.empty())
        {
            I.javadoc.reset();
            return;
        }

        // Check params size
        std::size_t const nNamedParams = std::ranges::
            count_if(I.Params, [](Param const& p) -> bool {
            return p.Name.has_value();
        });
        auto const documentedParams = getJavadocParamNames(*I.javadoc);
        if (nNamedParams != documentedParams.size())
        {
            I.javadoc.reset();
            return;
        }

        // Check param names
        if (!std::ranges::all_of(I.Params, [&](Param const& param) {
            if (param.Name)
            {
                return contains(documentedParams, *param.Name);
            }
            return true;
        }))
        {
            I.javadoc.reset();
        }
    }
}

void
JavadocFinalizer::
populateOverloadJavadoc(OverloadsInfo& I)
{
    // Create a view all Info members of I.
    // The javadoc for these function should already be as
    // complete as possible
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
                return infoPtr->asFunction();
            });
    if (!I.javadoc)
    {
        I.javadoc.emplace();
    }

    // briefs: populated in a previous step
    // blocks: we do not copy javadoc detail blocks because
    // it's impossible to guarantee that the details for
    // any of the functions make sense for all overloads.
    // We can only merge metadata.
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
finalizeJavadoc(Info& I)
{
    MRDOCS_CHECK_OR(!finalized_.contains(&I));
    finalized_.emplace(&I);
    ScopeExitRestore s(current_context_, &I);

    report::trace(
        "Finalizing javadoc for '{}'",
        corpus_.Corpus::qualifiedName(I));

    if (I.javadoc)
    {
        finalize(*I.javadoc);
    }
}

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
    if constexpr (requires { I.IntroducedName; })
    {
        finalize(I.IntroducedName);
    }
    if constexpr (requires { I.ShadowDeclarations; })
    {
        finalize(I.ShadowDeclarations);
    }
    if constexpr (requires { I.Deduced; })
    {
        finalize(I.Deduced);
    }
}

#define INFO(T) template void JavadocFinalizer::finalizeInfoData<T## Info>(T## Info&);
#include <mrdocs/Metadata/Info/InfoNodes.inc>

void
JavadocFinalizer::
finalize(doc::Reference& ref, bool const emitWarning)
{
    if (ref.id != SymbolID::invalid)
    {
        // Already resolved
        return;
    }
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
    processRelates(javadoc);
    copyDetails(javadoc);
    removeTempTextNodes(javadoc);
    trimBlocks(javadoc);
    unindentCodeBlocks(javadoc);
}

namespace {
bool
referenceCmp(
    doc::Reference const& lhs,
    doc::Reference const& rhs) {
    bool const lhsIsGlobal = lhs.string.starts_with("::");
    bool const rhsIsGlobal = rhs.string.starts_with("::");
    if (lhsIsGlobal != rhsIsGlobal)
    {
        return lhsIsGlobal < rhsIsGlobal;
    }
    std::size_t const lhsCount = std::ranges::count(lhs.string, ':');
    std::size_t const rhsCount = std::ranges::count(rhs.string, ':');
    if (lhsCount != rhsCount)
    {
        return lhsCount < rhsCount;
    }
    if (lhs.string != rhs.string)
    {
        return lhs.string < rhs.string;
    }
    return lhs.id < rhs.id;
}
}

void
JavadocFinalizer::
processRelates(Javadoc& javadoc)
{
    if (corpus_.config->autoRelates)
    {
        setAutoRelates();
    }

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
            std::string currentName = corpus_.Corpus::qualifiedName(current, relatedPtr->Parent);
            doc::Reference relatedRef(std::move(currentName));
            relatedRef.id = current_context_->id;
            // Insert in order by name
            auto const it = std::ranges::lower_bound(
                related.javadoc->related,
                relatedRef,
                referenceCmp);
            related.javadoc->related.insert(it, std::move(relatedRef));
        }
    }

    // Erase anything in the javadoc without a valid id
    std::erase_if(javadoc.relates, [](doc::Reference const& ref) {
        return !ref.id;
    });
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
            return static_cast<doc::UnorderedList const &>(*block).items.empty();
        }
        if (block->Kind == doc::NodeKind::heading)
        {
            return static_cast<doc::Heading const &>(*block).string.empty();
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
            { doc::NodeKind::copy_details });
    });
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
    if (!block.children.empty())
    {
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

namespace {
void
pushAllDerivedClasses(
    RecordInfo const* record,
    SmallVector<Info*, 16>& relatedRecordsOrEnums,
    CorpusImpl& corpus)
{
    for (auto& derivedId : record->Derived)
    {
        Info* derivedPtr = corpus.find(derivedId);
        MRDOCS_CHECK_OR_CONTINUE(derivedPtr);
        MRDOCS_CHECK_OR_CONTINUE(derivedPtr->Extraction == ExtractionMode::Regular);
        auto derived = dynamic_cast<RecordInfo*>(derivedPtr);
        MRDOCS_CHECK_OR_CONTINUE(derived);
        relatedRecordsOrEnums.push_back(derived);
        // Recursively get derived classes of the derived class
        pushAllDerivedClasses(derived, relatedRecordsOrEnums, corpus);
    }
}
}

void
JavadocFinalizer::
setAutoRelates()
{
    MRDOCS_ASSERT(current_context_);
    MRDOCS_CHECK_OR(current_context_->Extraction == ExtractionMode::Regular);
    MRDOCS_CHECK_OR(current_context_->isFunction());
    MRDOCS_CHECK_OR(current_context_->javadoc);
    auto& I = dynamic_cast<FunctionInfo&>(*current_context_);
    MRDOCS_CHECK_OR(!I.IsRecordMethod);
    auto* parentPtr = corpus_.find(I.Parent);
    MRDOCS_CHECK_OR(parentPtr);
    MRDOCS_CHECK_OR(parentPtr->isNamespace());

    auto toRecordOrEnum = [&](Polymorphic<TypeInfo> const& type) -> Info* {
        MRDOCS_CHECK_OR(type, nullptr);
        auto& innermost = innermostType(type);
        MRDOCS_CHECK_OR(innermost, nullptr);
        MRDOCS_CHECK_OR(innermost->isNamed(), nullptr);
        auto const& namedType = dynamic_cast<NamedTypeInfo const&>(*innermost);
        MRDOCS_CHECK_OR(namedType.Name, nullptr);
        SymbolID const namedSymbolID = namedType.Name->id;
        MRDOCS_CHECK_OR(namedSymbolID != SymbolID::invalid, nullptr);
        Info* infoPtr = corpus_.find(namedSymbolID);
        MRDOCS_CHECK_OR(infoPtr, nullptr);
        MRDOCS_CHECK_OR(
            infoPtr->isRecord() ||
            infoPtr->isEnum(), nullptr);
        return infoPtr;
    };

    SmallVector<Info*, 16> relatedRecordsOrEnums;

    // 1) Inner type of the first parameter
    [&] {
        MRDOCS_CHECK_OR(!I.Params.empty());
        auto* firstParamInfo = toRecordOrEnum(I.Params.front().Type);
        MRDOCS_CHECK_OR(firstParamInfo);
        if (firstParamInfo->Extraction == ExtractionMode::Regular)
        {
            relatedRecordsOrEnums.push_back(firstParamInfo);
        }
        // 2) If the type is a reference or a pointer, derived classes
        // of this inner type are also valid related records
        MRDOCS_CHECK_OR(firstParamInfo->isRecord());
        auto const* firstParamRecord = dynamic_cast<RecordInfo*>(firstParamInfo);
        MRDOCS_CHECK_OR(
            I.Params.front().Type->isLValueReference() ||
            I.Params.front().Type->isRValueReference() ||
            I.Params.front().Type->isPointer());
        // Get all transitively derived classes of firstParamRecord
       pushAllDerivedClasses(firstParamRecord, relatedRecordsOrEnums, corpus_);
    }();

    // 3) The return type of the function
    if (auto* returnTypeInfo = toRecordOrEnum(I.ReturnType))
    {
        if (returnTypeInfo->Extraction == ExtractionMode::Regular)
        {
            relatedRecordsOrEnums.push_back(returnTypeInfo);
        }
        // 4) If the return type is a template specialization,
        // and the template parameters are records, then
        // each template parameter is also a related record
        [&] {
            MRDOCS_CHECK_OR(I.ReturnType);
            MRDOCS_CHECK_OR(I.ReturnType->isNamed());
            auto& NTI = static_cast<NamedTypeInfo &>(*I.ReturnType);
            MRDOCS_CHECK_OR(NTI.Name);
            MRDOCS_CHECK_OR(NTI.Name->isSpecialization());
            auto const& NTIS = static_cast<SpecializationNameInfo &>(*NTI.Name);
            MRDOCS_CHECK_OR(!NTIS.TemplateArgs.empty());
            Polymorphic<TArg> const& firstArg = NTIS.TemplateArgs.front();
            MRDOCS_CHECK_OR(firstArg->isType());
            auto const& typeArg = static_cast<TypeTArg const &>(*firstArg);
            if (auto* argInfo = toRecordOrEnum(typeArg.Type))
            {
                if (argInfo->Extraction == ExtractionMode::Regular)
                {
                    relatedRecordsOrEnums.push_back(argInfo);
                }
            }
        }();
    }

    // Remove duplicates from relatedRecordsOrEnums
    std::ranges::sort(relatedRecordsOrEnums);
    relatedRecordsOrEnums.erase(
        std::ranges::unique(relatedRecordsOrEnums).begin(),
        relatedRecordsOrEnums.end());

    // Insert the records with valid ids into the javadoc relates section
    std::size_t const prevRelatesSize = I.javadoc->relates.size();
    for (Info const* relatedRecordOrEnumPtr : relatedRecordsOrEnums)
    {
        MRDOCS_CHECK_OR_CONTINUE(relatedRecordOrEnumPtr);
        MRDOCS_ASSERT(I.javadoc);
        Info const& recordOrEnum = *relatedRecordOrEnumPtr;
        MRDOCS_CHECK_OR_CONTINUE(recordOrEnum.Extraction == ExtractionMode::Regular);
        doc::Reference ref(recordOrEnum.Name);
        ref.id = recordOrEnum.id;

        // Check if already listed as friend
        if (auto* record = dynamic_cast<RecordInfo const*>(relatedRecordOrEnumPtr))
        {
            using std::views::transform;
            if (contains(transform(record->Friends, &FriendInfo::id), I.id))
            {
                // Already listed as a public friend
                continue;
            }
        }

        // Ensure no duplicates
        if (std::ranges::none_of(
                I.javadoc->relates,
                [&ref](doc::Reference const& otherRef) {
            return otherRef.string == ref.string || otherRef.id == ref.id;
        }))
        {
            // Insert in order by name
            auto const it = std::ranges::lower_bound(
                I.javadoc->relates.begin() + prevRelatesSize,
                I.javadoc->relates.end(),
                ref,
                referenceCmp);
            I.javadoc->relates.insert(it, std::move(ref));
        }
    }
}

void
JavadocFinalizer::
copyDetails(Javadoc& javadoc)
{
    for (auto blockIt = javadoc.blocks.begin(); blockIt != javadoc.blocks.end();)
    {
        // Get paragraph
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

        // Find copydetails command
        std::optional<doc::CopyDetails> copied;
        for (auto textIt = para.children.begin(); textIt != para.children.end();)
        {
            // Find copydoc command
            auto& text = *textIt;
            if (text->Kind != doc::NodeKind::copy_details)
            {
                ++textIt;
                continue;
            }
            // Copy reference
            copied = dynamic_cast<doc::CopyDetails&>(*text);

            // Remove copied node from the text
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

        // Nothing to copy: continue to the next block
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
        finalizeJavadoc(const_cast<Info&>(res));

        // Check if there's any documentation details to copy
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

        // Copy detail blocks from source to destination to
        // the same position in the destination
        Javadoc const& src = *res.javadoc;
        if (!src.blocks.empty())
        {
            blockIt = javadoc.blocks.insert(blockIt, src.blocks.begin(), src.blocks.end());
            blockIt += src.blocks.size();
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
      std::string locWarning =
          std::format("{}:{}\n", loc.FullPath, loc.LineNumber);
      int i = 1;
      for (auto const &msg : msgs) {
        locWarning += std::format("    {}) {}\n", i++, msg);
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
                "{}: Missing documentation for return value",
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
