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

#include <lib/Metadata/Finalizers/DocComment/Function.hpp>
#include <lib/Metadata/Finalizers/DocComment/Overloads.hpp>
#include <lib/Metadata/Finalizers/DocComment/parseInlines.hpp>
#include <lib/Metadata/Finalizers/DocCommentFinalizer.hpp>
#include <mrdocs/ADT/Overload.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Support/Algorithm.hpp>
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/ScopeExit.hpp>
#include <algorithm>
#include <format>

namespace mrdocs {

void
DocCommentFinalizer::
build()
{
    auto infos =
        corpus_.info_ |
        std::views::filter([](std::unique_ptr<Symbol> const& ptr) {
            return ptr && ptr->Extraction != ExtractionMode::Dependency;
        }) |
        std::views::transform([](std::unique_ptr<Symbol> const& ptr) -> Symbol& {
            MRDOCS_ASSERT(ptr);
            return *ptr;
        });

    // Finalize briefs:
    // We do it first because all other steps require accessing
    // the brief of other functions, these often need to be resolved
    // with @copybrief or auto-brief, and we need to ensure that
    // there are no circular dependencies for other metadata.
    for (auto& I : infos)
    {
        finalizeBrief(I);
    }

    // Finalize metadata:
    // A @copydetails command also implies we should copy
    // other metadata from the referenced symbol.
    // The metadata from other symbols includes things
    // like function parameters, return types, etc...
    // We copy this now because we need the complete metadata
    // for all objects to generate doc for overloads.
    // For instance, overloads cannot aggregate function
    // parameters as if the parameters are not resolved.
    for (auto& I : infos)
    {
        copyDetails(I);
    }

    // Create doc for overloads:
    // We do it before the references because the overloads
    // themselves can be used in the references. For instance,
    // `@ref foo` refers to the overload set because it doesn't
    // specify the function signature.
    if (corpus_.config->overloads)
    {
        for (auto& I : infos)
        {
            MRDOCS_CHECK_OR_CONTINUE(I.isOverloads());
            generateOverloadDocs(I.asOverloads());
        }
    }

    // Resolve references in the doc:
    // We do this before resolving overloads because a reference
    // to a function without signature should resolve to the
    // overload set, not to a specific function.
    for (auto& I : infos)
    {
        // Rename this to "finalizeReferences" and move other
        // functionality to other loops.
        resolveReferences(I);
    }

    // Populate trivial function metadata
    // - We do it after the overloads because they should not
    //   rely on metadata inherited from automatic generated doc
    // - We also do it after the references because some metadata
    //   might be resolved from references with @copydetails
    if (corpus_.config->autoFunctionMetadata)
    {
        for (auto& I : infos)
        {
            MRDOCS_CHECK_OR_CONTINUE(I.isFunction());
            generateAutoFunctionMetadata(I.asFunction());
        }
    }

    // Process relates
    for (auto& I : infos)
    {
        processRelates(I);
    }

    // Normalize siblings
    for (auto& I : infos)
    {
        normalizeSiblings(I);
    }

    // Tidy up doc
    for (auto& I : infos)
    {
        tidyUp(I);
    }

    // Parse inlines in terminal text nodes
    for (auto& I : infos)
    {
        parseInlines(I);
    }

    // Remove invalid references
    for (auto& I : infos)
    {
        removeInvalidReferences(I);
    }

    // - Emitting param warning require everything to be completely
    //   processed
    emitWarnings();
}

void
DocCommentFinalizer::
finalizeBrief(Symbol& I)
{
    MRDOCS_CHECK_OR(!finalized_brief_.contains(&I));
    finalized_brief_.emplace(&I);

    report::trace(
            "Finalizing brief for '{}'",
            corpus_.Corpus::qualifiedName(I));

    if (I.isOverloads())
    {
        // Overloads are expected not to have doc.
        // We'll create a doc for them if they don't have one.
        if (!I.doc)
        {
            I.doc.emplace();
        }
        // The brief of an overload is always empty
        auto& OI = I.asOverloads();
        for (auto const& MemberIDs = OI.Members;
             auto& memberID : MemberIDs)
        {
            Symbol* member = corpus_.find(memberID);
            MRDOCS_CHECK_OR_CONTINUE(member);
            finalizeBrief(*member);
        }
        auto functions = overloadFunctionsRange(OI, corpus_);
        populateOverloadsBrief(OI, functions, corpus_);
        return;
    }

    MRDOCS_CHECK_OR(I.doc);
    auto& doc = *I.doc;
    // Copy brief from other symbols if there's a @copydoc
    copyBrief(I, doc);
    // Set auto brief if brief is still empty
    setAutoBrief(doc);
}

void
DocCommentFinalizer::
copyBrief(Symbol const& ctx, DocComment& doc)
{
    MRDOCS_CHECK_OR(doc.brief);
    MRDOCS_CHECK_OR(!doc.brief->copiedFrom.empty());
    MRDOCS_CHECK_OR(doc.brief->children.empty());

    for (std::string const& ref: doc.brief->copiedFrom)
    {
        // Look for source
        auto resRef = corpus_.lookup(ctx.id, ref);

        // Check if the source exists
        if (!resRef)
        {
            if (corpus_.config->warnings &&
                corpus_.config->warnBrokenRef &&
                !refWarned_.contains({ref, ctx.Name}))
            {
                this->warn(
                    ctx,
                    "{}: Failed to copy brief from '{}' (symbol not found)\n"
                    "    {}",
                    corpus_.Corpus::qualifiedName(ctx),
                    ref,
                    resRef.error().reason());
            }
            continue;
        }

        // Ensure the brief source is finalized
        Symbol const& res = *resRef;
        finalizeBrief(const_cast<Symbol&>(res));

        // Check if the source has a brief
        if (!res.doc ||
            !res.doc->brief.has_value())
        {
            if (corpus_.config->warnings &&
                corpus_.config->warnBrokenRef &&
                !refWarned_.contains({ref, ctx.Name}))
            {
                auto resPrimaryLoc = getPrimaryLocation(res);
                this->warn(
                    ctx,
                    "{}: Failed to copy brief from {} '{}' (no brief available).\n"
                    "    No brief available.\n"
                    "        {}:{}\n"
                    "        Note: No brief available for '{}'.",
                    corpus_.Corpus::qualifiedName(ctx),
                    toString(res.Kind),
                    ref,
                    resPrimaryLoc->FullPath,
                    resPrimaryLoc->LineNumber,
                    corpus_.Corpus::qualifiedName(res));
            }
            continue;
        }

        DocComment const& src = *res.doc;
        doc.brief->children = src.brief->children;
        return;
    }
}

void
DocCommentFinalizer::
setAutoBrief(DocComment& doc) const
{
    MRDOCS_CHECK_OR(corpus_.config->autoBrief);
    MRDOCS_CHECK_OR(!doc.brief);
    MRDOCS_CHECK_OR(!doc.Document.empty());

    auto isInvalidBriefText = [](
        Polymorphic<doc::Inline> const& el)
        {
            MRDOCS_ASSERT(!el.valueless_after_move());
            return !el->isText() ||
                   el->asText().literal.empty() ||
                   el->asText().Kind == doc::InlineKind::CopyDetails ||
                   isWhitespace(el->asText().literal);
        };

    for (auto it = doc.Document.begin(); it != doc.Document.end();)
    {
        if (auto& block = *it;
            block->Kind == doc::BlockKind::Paragraph)
        {
            auto& para = dynamic_cast<doc::ParagraphBlock&>(*block);
            if (std::ranges::all_of(para.children, isInvalidBriefText))
            {
                ++it;
                continue;
            }
            doc.brief.emplace();
            doc.brief->children = para.children;
            it = doc.Document.erase(it);
            return;
        }
        ++it;
    }
}

void
DocCommentFinalizer::
copyDetails(Symbol& I)
{
    MRDOCS_CHECK_OR(!finalized_metadata_.contains(&I));
    finalized_metadata_.emplace(&I);

    report::trace(
            "Finalizing metadata for '{}'",
            corpus_.Corpus::qualifiedName(I));

    MRDOCS_CHECK_OR(I.doc);
    MRDOCS_CHECK_OR(!I.doc->Document.empty());
    DocComment& destDoc = *I.doc;

    llvm::SmallVector<doc::CopyDetailsInline, 16> copiedRefs;
    for (auto& block: destDoc.Document)
    {
        MRDOCS_CHECK_OR_CONTINUE(block->isParagraph());
        auto& para = dynamic_cast<doc::ParagraphBlock&>(*block);
        MRDOCS_CHECK_OR_CONTINUE(!para.children.empty());

        for (auto& text: para.children)
        {
            MRDOCS_CHECK_OR_CONTINUE(text->isCopyDetails());
            copiedRefs.emplace_back(dynamic_cast<doc::CopyDetailsInline&>(*text));
        }
        MRDOCS_CHECK_OR_CONTINUE(!copiedRefs.empty());
    }

    for (doc::CopyDetailsInline const& copied: copiedRefs)
    {
        // Find element
        auto resRef = corpus_.lookup(I.id, copied.string);
        if (!resRef)
        {
            if (corpus_.config->warnings &&
                corpus_.config->warnBrokenRef &&
                !refWarned_.contains({copied.string, I.Name}))
            {
                this->warn(
                    I,
                    "{}: Failed to copy metadata from '{}' (symbol not found)\n"
                    "    {}",
                    corpus_.Corpus::qualifiedName(I),
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
        auto copyInfoRangeMetadata = [&](llvm::ArrayRef<Symbol const*> srcInfoPtrs)
        {
            auto srcInfos = srcInfoPtrs
                            | std::views::transform(
                                [](Symbol const* ptr) -> Symbol const& {
                return *ptr;
            });

            // Ensure the source metadata is finalized
            for (auto& srcInfo: srcInfos)
            {
                auto& mutSrcInfo = const_cast<Symbol&>(srcInfo);
                copyDetails(mutSrcInfo);
            }

            // Copy returns only if destination is empty
            if (destDoc.returns.empty())
            {
                for (auto const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (doc::ReturnsBlock const& returnsEl: srcInfo.doc->returns)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destDoc.returns, returnsEl));
                        destDoc.returns.push_back(returnsEl);
                    }
                }
            }

            // Copy only params that don't exist at the destination
            // documentation but that do exist in the destination
            // function parameters declaration.
            if (I.isFunction())
            {
                auto& destF = I.asFunction();
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.isFunction());
                    auto const& srcFunction = srcInfo.asFunction();
                    MRDOCS_CHECK_OR_CONTINUE(srcFunction.doc);
                    for (DocComment const& srcDocComment = *srcFunction.doc;
                         auto const& srcDocParam: srcDocComment.params)
                    {
                        // check if param doc doesn't already exist
                        MRDOCS_CHECK_OR_CONTINUE(
                            std::ranges::none_of(
                                destDoc.params,
                                [&srcDocParam](doc::ParamBlock const& destDocParam) {
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
                        destDoc.params.push_back(srcDocParam);
                    }
                }
            }

            // Copy only tparams that don't exist at the destination
            // documentation but that do exist in the destination
            // template parameters.
            auto getTemplateInfo = [](Symbol& I) -> TemplateInfo const*
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
            };


            if (auto const destTemplateInfo = getTemplateInfo(I))
            {
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (DocComment const& srcDocComment = *srcInfo.doc;
                         auto const& srcTParam: srcDocComment.tparams)
                    {
                        // tparam doesn't already exist at the destination
                        MRDOCS_CHECK_OR_CONTINUE(
                            std::ranges::none_of(
                                destDoc.tparams,
                                [&srcTParam](doc::TParamBlock const& destTParam) {
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
                        destDoc.tparams.push_back(srcTParam);
                    }
                }
            }

            // Copy exceptions only if destination exceptions are empty
            // and the destination is not noexcept
            bool const destIsNoExcept =
                I.isFunction() &&
                I.asFunction().Noexcept.Kind == NoexceptKind::False;
            if (destDoc.exceptions.empty() &&
                !destIsNoExcept)
            {
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (doc::ThrowsBlock const& exceptionsEl: srcInfo.doc->exceptions)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destDoc.exceptions, exceptionsEl));
                        destDoc.exceptions.push_back(exceptionsEl);
                    }
                }
            }

            // Copy sees only if destination sees are empty
            if (destDoc.sees.empty())
            {
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (doc::SeeBlock const& seesEl: srcInfo.doc->sees)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destDoc.sees, seesEl));
                        destDoc.sees.push_back(seesEl);
                    }
                }
            }

            // Copy preconditions only if destination preconditions is empty
            if (destDoc.preconditions.empty())
            {
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (doc::PreconditionBlock const& preconditionsEl: srcInfo.doc->preconditions)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(destDoc.preconditions, preconditionsEl));
                        destDoc.preconditions.push_back(preconditionsEl);
                    }
                }
            }

            // Copy postconditions only if destination postconditions is empty
            if (destDoc.postconditions.empty())
            {
                for (Symbol const& srcInfo: srcInfos)
                {
                    MRDOCS_CHECK_OR_CONTINUE(srcInfo.doc);
                    for (doc::PostconditionBlock const& postconditionsEl:
                         srcInfo.doc->postconditions)
                    {
                        MRDOCS_CHECK_OR_CONTINUE(!contains(
                            destDoc.postconditions,
                            postconditionsEl));
                        destDoc.postconditions.push_back(postconditionsEl);
                    }
                }
            }
        };

        // Ensure the source metadata is finalized
        Symbol const& res = *resRef;
        if (!res.isOverloads())
        {
            // If it's a single element, we check the element doc
            if (!res.doc)
            {
                if (corpus_.config->warnings &&
                    corpus_.config->warnBrokenRef &&
                    !refWarned_.contains({copied.string, I.Name}))
                {
                    auto resPrimaryLoc = getPrimaryLocation(res);
                    this->warn(
                        I,
                        "{}: Failed to copy metadata from {} '{}' (no documentation available).\n"
                        "    No metadata available.\n"
                        "        {}:{}\n"
                        "        Note: No documentation available for '{}'.",
                        corpus_.Corpus::qualifiedName(I),
                        toString(res.Kind),
                        copied.string,
                        resPrimaryLoc->FullPath,
                        resPrimaryLoc->LineNumber,
                        corpus_.Corpus::qualifiedName(res));
                }
                continue;
            }
            llvm::SmallVector<Symbol const*, 1> srcInfos = { &res };
            copyInfoRangeMetadata(srcInfos);
        }
        else
        {
            auto& OI = res.asOverloads();
            llvm::SmallVector<Symbol const*, 16> srcInfos;
            srcInfos.reserve(OI.Members.size());
            for (auto& memberID: OI.Members)
            {
                Symbol* member = corpus_.find(memberID);
                MRDOCS_CHECK_OR_CONTINUE(member);
                srcInfos.push_back(member);
            }
            copyInfoRangeMetadata(srcInfos);
        }
    }

    if (I.doc)
    {
        copyDetails(I,*I.doc);
    }
}

void
DocCommentFinalizer::
copyDetails(Symbol const& ctx, DocComment& doc)
{
    for (auto blockIt = doc.Document.begin(); blockIt != doc.Document.end();)
    {
        // Get paragraph
        auto& block = *blockIt;
        if (!block->isParagraph())
        {
            ++blockIt;
            continue;
        }
        auto& para = block->asParagraph();
        if (para.empty())
        {
            ++blockIt;
            continue;
        }

        // Find copydetails command
        Optional<doc::CopyDetailsInline> copied;
        for (auto inlineIt = para.children.begin(); inlineIt != para.children.end();)
        {
            // Find copydoc command
            auto& inlineEl = *inlineIt;
            if (!inlineEl->isCopyDetails())
            {
                ++inlineIt;
                continue;
            }
            // Copy reference
            copied = inlineEl->asCopyDetails();

            // Remove copied node from the inlineEl
            /* it2 = */ para.children.erase(inlineIt);
            break;
        }

        // Trim the paragraph after removing the copydetails command
        doc::trim(para.asInlineContainer());

        // Remove empty children from the paragraph
        std::erase_if(para.children, [](Polymorphic<doc::Inline> const& child) {
            return doc::isEmpty(child);
        });

        // We should merge consecutive text nodes that have exactly the
        // same terminal kind

        // Remove the entire paragraph block from the doc if it is empty
        if (para.empty())
        {
            blockIt = doc.Document.erase(blockIt);
            MRDOCS_CHECK_OR_CONTINUE(copied);
        }

        // Nothing to copy: continue to the next block
        if (!copied)
        {
            ++blockIt;
            continue;
        }

        // Find the node to copy from
        auto resRef = corpus_.lookup(ctx.id, copied->string);
        if (!resRef)
        {
            if (corpus_.config->warnings &&
                corpus_.config->warnBrokenRef &&
                !refWarned_.contains({copied->string, ctx.Name}))
            {
                this->warn(
                    ctx,
                    "{}: Failed to copy documentation from '{}' (symbol not found)\n"
                    "    {}",
                    corpus_.Corpus::qualifiedName(ctx),
                    copied->string,
                    resRef.error().reason());
            }
            continue;
        }

        // Ensure the source node is finalized
        Symbol const& res = *resRef;
        resolveReferences(const_cast<Symbol&>(res));

        // Check if there's any documentation details to copy
        if (!res.doc)
        {
            if (corpus_.config->warnings &&
                corpus_.config->warnBrokenRef &&
                !refWarned_.contains({copied->string, ctx.Name}))
            {
                auto resPrimaryLoc = getPrimaryLocation(res);
                this->warn(
                    ctx,
                    "{}: Failed to copy documentation from {} '{}' (no documentation available).\n"
                    "    No documentation available.\n"
                    "        {}:{}\n"
                    "        Note: No documentation available for '{}'.",
                    corpus_.Corpus::qualifiedName(ctx),
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
        DocComment const& src = *res.doc;
        if (!src.Document.empty())
        {
            blockIt = doc.Document.insert(blockIt, src.Document.begin(), src.Document.end());
            blockIt += src.Document.size();
        }
    }
}

void
DocCommentFinalizer::generateOverloadDocs(OverloadsSymbol& I)
{
    if (!I.doc)
    {
        I.doc.emplace();
    }

    // Create a view all Info members of I.
    // The doc for these function should already be as
    // complete as possible
    auto functions =
        I.Members |
        std::views::transform([&](SymbolID const& id)
            {
                return corpus_.find(id);
            }) |
        std::views::filter([](Symbol const* infoPtr)
            {
                return infoPtr != nullptr && infoPtr->isFunction();
            }) |
        std::views::transform([](Symbol const* infoPtr) -> FunctionSymbol const&
            {
                return infoPtr->asFunction();
            });
    if (!I.doc)
    {
        I.doc.emplace();
    }

    // briefs: populated in a previous step
    // blocks: we do not copy doc detail blocks because
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
DocCommentFinalizer::resolveReferences(Symbol& I)
{
    MRDOCS_CHECK_OR(!finalized_.contains(&I));
    finalized_.emplace(&I);

    report::trace(
        "Finalizing doc for '{}'",
        corpus_.Corpus::qualifiedName(I));

    if (I.doc)
    {
        auto& doc = *I.doc;
        bottomUpTraverse(doc, makeOverload(
          [this, &I](doc::ReferenceInline& node) { this->resolveReference(I, node, true); },
          [this, &I](doc::ThrowsBlock& node) { this->resolveReference(I, node.exception, false); }));
    }
}

void
DocCommentFinalizer::resolveReference(
    Symbol const& ctx,
    doc::ReferenceInline& ref,
    bool const emitWarning)
{
    if (ref.id != SymbolID::invalid)
    {
        // Already resolved
        return;
    }
    if (auto resRef = corpus_.lookup(ctx.id, ref.literal))
    {
        // KRYSTIAN NOTE: We should provide an overload that
        // returns a non-const reference.
        auto& res = const_cast<Symbol&>(*resRef);
        ref.id = res.id;
    }
    else if (
        emitWarning &&
        corpus_.config->warnings &&
        corpus_.config->warnBrokenRef &&
        // Only warn once per reference
        !refWarned_.contains({ref.literal, ctx.Name}) &&
        // Ignore std:: references
        !ref.literal.starts_with("std::"))
    {
        this->warn(
            ctx,
            "{}: Failed to resolve reference to '{}'\n"
            "    {}",
            corpus_.Corpus::qualifiedName(ctx),
            ref.literal,
            resRef.error().reason());
        refWarned_.insert({ref.literal, ctx.Name});
    }
}

void
DocCommentFinalizer::
generateAutoFunctionMetadata(FunctionSymbol& I) const
{
    // For special functions (constructors, destructors, ...),
    // we create the doc if it does not exist because
    // we can populate all the fields from the function category.
    // For other types of functions, we'll only populate
    // the missing fields when the doc already exists.
    bool const isSpecial = isSpecialFunction(I);
    MRDOCS_CHECK_OR(isSpecial || I.doc);
    bool forceEmplaced = false;
    if (isSpecial &&
        !I.doc)
    {
        I.doc.emplace();
        forceEmplaced = true;
    }

    // Populate a missing doc brief
    populateFunctionBrief(I, corpus_);

    // Populate a missing doc returns
    populateFunctionReturns(I, corpus_);

    // Populate missing doc params
    populateFunctionParams(I, corpus_);

    // If we forcefully created the doc, we need to
    // check if the function was able to populate all the
    // fields. If not, we'll remove the doc.
    if (forceEmplaced)
    {
        // Check brief and returns
        if (!I.doc->brief)
        {
            I.doc.reset();
            return;
        }

        if (!is_one_of(I.Class, {
            FunctionClass::Constructor,
            FunctionClass::Destructor }) &&
            I.doc->returns.empty())
        {
            I.doc.reset();
            return;
        }

        // Check params size
        std::size_t const nNamedParams = std::ranges::
            count_if(I.Params, [](Param const& p) -> bool {
            return p.Name.has_value();
        });
        auto const documentedParams = getDocCommentParamNames(*I.doc);
        if (nNamedParams != documentedParams.size())
        {
            I.doc.reset();
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
            I.doc.reset();
        }
    }
}

namespace {
// Comparison function for reference to keep the related
// references sorted by name.
bool
referenceCmp(
    doc::ReferenceInline const& lhs,
    doc::ReferenceInline const& rhs) {
    bool const lhsIsGlobal = lhs.literal.starts_with("::");
    bool const rhsIsGlobal = rhs.literal.starts_with("::");
    if (lhsIsGlobal != rhsIsGlobal)
    {
        return lhsIsGlobal < rhsIsGlobal;
    }
    std::size_t const lhsCount = std::ranges::count(lhs.literal, ':');
    std::size_t const rhsCount = std::ranges::count(rhs.literal, ':');
    if (lhsCount != rhsCount)
    {
        return lhsCount < rhsCount;
    }
    if (lhs.literal != rhs.literal)
    {
        return lhs.literal < rhs.literal;
    }
    return lhs.id < rhs.id;
}
}

void
DocCommentFinalizer::
processRelates(Symbol& ctx, DocComment& doc)
{
    if (corpus_.config->autoRelates)
    {
        setAutoRelates(ctx);
    }

    MRDOCS_CHECK_OR(!doc.relates.empty());

    Symbol const* currentPtr = corpus_.find(ctx.id);
    MRDOCS_ASSERT(currentPtr);
    Symbol const& current = *currentPtr;

    if (!current.isFunction())
    {
        this->warn(
            ctx,
            "{}: `@relates` only allowed for functions",
            corpus_.Corpus::qualifiedName(current));
        doc.relates.clear();
        return;
    }

    for (doc::ReferenceInline& ref: doc.relates)
    {
        resolveReference(ctx, ref, true);
        Symbol* relatedPtr = corpus_.find(ref.id);
        MRDOCS_CHECK_OR_CONTINUE(relatedPtr);
        Symbol& related = *relatedPtr;
        if (!related.doc)
        {
            related.doc.emplace();
        }
        if (std::ranges::none_of(
            related.doc->related,
               [&ctx](doc::ReferenceInline const& otherRef) {
                    return otherRef.id == ctx.id;
                }))
        {
            std::string currentName = corpus_.Corpus::qualifiedName(current, relatedPtr->Parent);
            doc::ReferenceInline relatedRef(std::move(currentName));
            relatedRef.id = ctx.id;
            // Insert in order by name
            auto const it = std::ranges::lower_bound(
                related.doc->related,
                relatedRef,
                referenceCmp);
            related.doc->related.insert(it, std::move(relatedRef));
        }
    }

    // Erase anything in the doc without a valid id
    std::erase_if(doc.relates, [](doc::ReferenceInline const& ref) {
        return !ref.id;
    });
}

namespace {
void
pushAllDerivedClasses(
    RecordSymbol const* record,
    llvm::SmallVector<Symbol*, 16>& relatedRecordsOrEnums,
    CorpusImpl& corpus)
{
    for (auto& derivedId : record->Derived)
    {
        Symbol* derivedPtr = corpus.find(derivedId);
        MRDOCS_CHECK_OR_CONTINUE(derivedPtr);
        MRDOCS_CHECK_OR_CONTINUE(derivedPtr->Extraction == ExtractionMode::Regular);
        auto derived = dynamic_cast<RecordSymbol*>(derivedPtr);
        MRDOCS_CHECK_OR_CONTINUE(derived);
        relatedRecordsOrEnums.push_back(derived);
        // Recursively get derived classes of the derived class
        pushAllDerivedClasses(derived, relatedRecordsOrEnums, corpus);
    }
}
}

void
DocCommentFinalizer::
setAutoRelates(Symbol& ctx)
{
    MRDOCS_CHECK_OR(ctx.Extraction == ExtractionMode::Regular);
    MRDOCS_CHECK_OR(ctx.isFunction());
    MRDOCS_CHECK_OR(ctx.doc);
    auto& I = ctx.asFunction();
    MRDOCS_CHECK_OR(!I.IsRecordMethod);
    auto* parentPtr = corpus_.find(I.Parent);
    MRDOCS_CHECK_OR(parentPtr);
    MRDOCS_CHECK_OR(parentPtr->isNamespace());

    auto toRecordOrEnum = [&](Polymorphic<Type> const& type) -> Symbol* {
        MRDOCS_CHECK_OR(type, nullptr);
        auto& innermost = innermostType(type);
        MRDOCS_CHECK_OR(innermost, nullptr);
        MRDOCS_CHECK_OR(innermost->isNamed(), nullptr);
        auto const& namedType = dynamic_cast<NamedType const&>(*innermost);
        MRDOCS_CHECK_OR(namedType.Name, nullptr);
        SymbolID const namedSymbolID = namedType.Name->id;
        MRDOCS_CHECK_OR(namedSymbolID != SymbolID::invalid, nullptr);
        Symbol* infoPtr = corpus_.find(namedSymbolID);
        MRDOCS_CHECK_OR(infoPtr, nullptr);
        MRDOCS_CHECK_OR(
            infoPtr->isRecord() ||
            infoPtr->isEnum(), nullptr);
        return infoPtr;
    };

    llvm::SmallVector<Symbol*, 16> relatedRecordsOrEnums;

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
        auto const* firstParamRecord = dynamic_cast<RecordSymbol*>(firstParamInfo);
        MRDOCS_CHECK_OR(
            I.Params.front().Type->isLValueReference() ||
            I.Params.front().Type->isRValueReference() ||
            I.Params.front().Type->isPointer());
        // Get all transitively derived classes of firstParamRecord
        pushAllDerivedClasses(firstParamRecord, relatedRecordsOrEnums, corpus_);
    }();

    // 3) The return type of the function
    if (auto* returnType = toRecordOrEnum(I.ReturnType))
    {
        if (returnType->Extraction == ExtractionMode::Regular)
        {
            relatedRecordsOrEnums.push_back(returnType);
        }
        // 4) If the return type is a template specialization,
        // and the template parameters are records, then
        // each template parameter is also a related record
        [&] {
            MRDOCS_CHECK_OR(I.ReturnType);
            MRDOCS_CHECK_OR(I.ReturnType->isNamed());
            auto& NTI = dynamic_cast<NamedType &>(*I.ReturnType);
            MRDOCS_CHECK_OR(NTI.Name);
            MRDOCS_CHECK_OR(NTI.Name->isSpecialization());
            auto const& NTIS = dynamic_cast<SpecializationName &>(*NTI.Name);
            MRDOCS_CHECK_OR(!NTIS.TemplateArgs.empty());
            Polymorphic<TArg> const& firstArg = NTIS.TemplateArgs.front();
            MRDOCS_CHECK_OR(firstArg->isType());
            auto const& typeArg = dynamic_cast<TypeTArg const &>(*firstArg);
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

    // Insert the records with valid ids into the doc relates section
    std::size_t const prevRelatesSize = I.doc->relates.size();
    for (Symbol const* relatedRecordOrEnumPtr : relatedRecordsOrEnums)
    {
        MRDOCS_CHECK_OR_CONTINUE(relatedRecordOrEnumPtr);
        MRDOCS_ASSERT(I.doc);
        Symbol const& recordOrEnum = *relatedRecordOrEnumPtr;
        MRDOCS_CHECK_OR_CONTINUE(recordOrEnum.Extraction == ExtractionMode::Regular);
        doc::ReferenceInline ref(recordOrEnum.Name);
        ref.id = recordOrEnum.id;

        // Check if already listed as friend
        if (auto* record = dynamic_cast<RecordSymbol const*>(relatedRecordOrEnumPtr))
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
                I.doc->relates,
                [&ref](doc::ReferenceInline const& otherRef) {
            return otherRef.literal == ref.literal || otherRef.id == ref.id;
        }))
        {
            // Insert in order by name
            auto const it = std::ranges::lower_bound(
                I.doc->relates.begin() + prevRelatesSize,
                I.doc->relates.end(),
                ref,
                referenceCmp);
            I.doc->relates.insert(it, std::move(ref));
        }
    }
}

void
DocCommentFinalizer::
tidyUp(DocComment& doc)
{
    // Bottom-up traversal cleaning up the doc
    bottomUpTraverse(doc, []<class NodeTy>(NodeTy& node) {
        // Remove any @copy* nodes that got left behind
        if constexpr (requires { { node.children } -> range_of<Polymorphic<doc::Inline>>; })
        {
            std::erase_if(node.children, [](Polymorphic<doc::Inline> const& el)
            {
                return el->isCopyDetails();
            });
        }

        // - Trim leading and trailing empty inlines in the node
        // - Merging consecutive empty blocks (like HTML whitespace normalization)
        //   To be implemented and improved as needed
        if constexpr (std::derived_from<NodeTy, doc::Block>)
        {
            doc::trim(node.asBlock());
        }

        // Remove consecutive whitespace characters in text nodes
        if constexpr (std::same_as<NodeTy, doc::TextInline>)
        {
            auto& textNode = static_cast<doc::TextInline&>(node);
            std::string_view sv = textNode.literal;

            // Early out if there is NO consecutive whitespace.
            auto it = std::ranges::adjacent_find(sv, [](char a, char b) {
                return isWhitespace(a) && isWhitespace(b);
            });
            if (it == sv.end())
            {
                return;
            }

            std::string out;
            out.reserve(sv.size());

            bool lastWasSpace = false; // whether we last EMITTED a space
            for (char c: sv)
            {
                if (isWhitespace(c))
                {
                    if (!lastWasSpace)
                    {
                        out.push_back(' ');
                        lastWasSpace = true;
                    }
                }
                else
                {
                    out.push_back(c);
                    lastWasSpace = false;
                }
            }

            textNode.literal = std::move(out);
        }

        // - Remove any child blocks or inlines without content
        //   (especially after we do the trimming bottom up)
        if constexpr (requires { { node.children } -> range_of<Polymorphic<doc::Inline>>; })
        {
            std::erase_if(node.children, [](Polymorphic<doc::Inline> const& el)
            {
                return isEmpty(el);
            });
        }
        if constexpr (requires { { node.blocks } -> range_of<Polymorphic<doc::Block>>; })
        {
            std::erase_if(node.blocks, [](Polymorphic<doc::Block> const& el)
            {
                return isEmpty(el);
            });
        }
        if constexpr (requires { { node.Document } -> range_of<Polymorphic<doc::Block>>; })
        {
            std::erase_if(node.Document, [](Polymorphic<doc::Block> const& el)
            {
                return isEmpty(el);
            });
        }

        // - Unindenting code blocks (but not Code inlines)
        if constexpr (std::same_as<NodeTy, doc::CodeBlock>)
        {
            auto& codeBlock = static_cast<doc::CodeBlock&>(node);
            codeBlock.literal = reindentCode(codeBlock.literal, 0);
        }
    });

    // Remove elements of main DocComment that happen to be empty after trimming
    // Lambda that takes a vector of T and removes elements for which isEmpty returns true
    auto removeEmpty = [](auto& vec) {
        std::erase_if(vec, [](auto const& el) {
            return isEmpty(el);
        });
    };
    removeEmpty(doc.Document);
    removeEmpty(doc.returns);
    removeEmpty(doc.params);
    removeEmpty(doc.tparams);
    removeEmpty(doc.exceptions);
    removeEmpty(doc.sees);
    removeEmpty(doc.preconditions);
    removeEmpty(doc.postconditions);
    // removeEmpty(doc.relates);
    // removeEmpty(doc.related);
    if (doc.brief && isEmpty(*doc.brief))
    {
        doc.brief.reset();
    }
}

void
DocCommentFinalizer::
normalizeSiblings(DocComment& doc)
{
    // Bottom-up traversal cleaning up the doc
    bottomUpTraverse(doc, [](doc::InlineContainer& node) {
        // Only containers with inline children can participate in merging
        // (1) Optional: flatten trivial same-type single-child wrappers
        //     e.g. <mono><mono>...</mono></mono> → <mono>...</mono>
        // We do this locally for each child to prevent unnecessary barriers
        // to sibling merge.
        for (auto& ch: node.children)
        {
            visit(*ch, []<class InlineTy>(InlineTy& inl) {
                auto* outer = dynamic_cast<doc::InlineContainer*>(&inl);
                MRDOCS_CHECK_OR(outer);
                MRDOCS_CHECK_OR(outer->children.size() == 1);
                auto& only = outer->children.front();
                MRDOCS_CHECK_OR(only->Kind == inl.Kind);
                auto* only_inner = dynamic_cast<doc::InlineContainer*>(&*only);
                MRDOCS_CHECK_OR(only_inner);
                // Move grandchildren up into outer
                outer->children.insert(
                    outer->children.end(),
                    std::make_move_iterator(only_inner->children.begin()),
                    std::make_move_iterator(only_inner->children.end()));
                only_inner->children.clear();
                // make `only` a moved-from node to be removed later
                // (we can't just reset/move it out because we're in a
                // reference to it in the vector)
                auto tmp = std::move(only);
            });
        }
        // Filter out any nulls created by the flatten step
        std::erase_if(node.children, [](Polymorphic<doc::Inline> const& el) {
            return el.valueless_after_move();
        });

        // (2) Single left→right pass that coalesces adjacent siblings
        //     - Text + Text: concatenate
        //     - Same-kind wrappers: move-append children
        //       (attributes must match if you model them; keep the check
        //       next to Kind)
        std::vector<Polymorphic<doc::Inline>> out;
        out.reserve(node.children.size());
        auto can_merge_same_kind =
            [](doc::Inline const& a, doc::Inline const& b) {
            // Filter out kinds that don't make sense to merge,
            // like images and links.
            return a.Kind == b.Kind
                   && !is_one_of(
                       a.Kind,
                       { doc::InlineKind::Link,
                         doc::InlineKind::Image,
                         doc::InlineKind::LineBreak,
                         doc::InlineKind::SoftBreak });
        };
        for (auto& cur: node.children)
        {
            MRDOCS_ASSERT(!cur.valueless_after_move());

            if (!out.empty())
            {
                auto& prev = out.back();

                // Text + Text
                if (prev->isText() &&
                    cur->isText())
                {
                    prev->asText().literal += cur->asText().literal;
                    // drop cur
                    continue;
                }

                // Same-kind wrappers: merge containers by moving children
                if (can_merge_same_kind(*prev, *cur))
                {
                    // Try to view both as InlineContainer (non-text
                    // wrappers should be)
                    auto* pc = dynamic_cast<doc::InlineContainer*>(&*prev);
                    auto* cc = dynamic_cast<doc::InlineContainer*>(&*cur);
                    if (pc && cc)
                    {
                        pc->children.insert(
                            pc->children.end(),
                            std::make_move_iterator(cc->children.begin()),
                            std::make_move_iterator(cc->children.end()));
                        cc->children.clear();
                        // merged; drop cur
                        continue;
                    }
                }
            }

            out.emplace_back(std::move(cur));
        }

        node.children = std::move(out);
    });
}

void
DocCommentFinalizer::
parseInlines(DocComment& doc)
{
    bottomUpTraverse(doc, []<std::derived_from<doc::InlineContainer> NodeTy>(NodeTy& node) {
        if constexpr (requires { { node.children } -> range_of<Polymorphic<doc::Inline>>; })
        {
            auto it = node.children.begin();
            while (it != node.children.end())
            {
                Polymorphic<doc::Inline>& el = *it;

                // Advance when doesn't text
                if (!el->isText()) {
                    ++it;
                    continue;
                }

                auto& textEl = el->asText();
                doc::InlineContainer v;
                ParseResult r = parse(textEl.literal, v);

                // advance on parse failure
                if (!r)
                {
                    ++it;
                    continue;
                }

                // Remove the original text node; 'it' becomes the
                // insertion position.
                it = node.children.erase(it);

                // Move-insert each parsed child;
                // advance using returned iterators.
                for (auto& child : v.children)
                {
                    it = node.children.insert(it, std::move(child));
                    ++it;
                }
            }
        }
    });
}

namespace {
// A function erases all references in a vector that don't exist
// in the corpus with invalid references.
inline void
removeInvalidIds(CorpusImpl& corpus, std::vector<SymbolID>& refs)
{
    std::erase_if(refs, [&corpus](SymbolID const& id) {
        if (id == SymbolID::invalid)
        {
            return true;
        }
        if (!corpus.find(id))
        {
            return true;
        }
        return false;
    });
}

inline void
removeInvalidIds(CorpusImpl& corpus, std::vector<struct Name>& refs)
{
    std::erase_if(refs, [&corpus](struct Name const& N) {
        if (N.id == SymbolID::invalid)
        {
            return true;
        }
        if (!corpus.find(N.id))
        {
            return true;
        }
        if (N.isSpecialization())
        {
            if (!corpus.find(N.asSpecialization().specializationID))
            {
                return true;
            }
        }
        return false;
    });
}

inline void
removeInvalidIds(CorpusImpl& corpus, std::vector<doc::ReferenceInline>& refs)
{
    std::erase_if(refs, [&corpus](doc::ReferenceInline const& ref) {
        if (ref.id == SymbolID::invalid)
        {
            return true;
        }
        if (!corpus.find(ref.id))
        {
            return true;
        }
        return false;
    });
}

inline void
removeInvalidIds(CorpusImpl& corpus, TemplateInfo& T)
{
    if (T.Primary != SymbolID::invalid &&
        !corpus.find(T.Primary))
    {
        T.Primary = SymbolID::invalid;
    }
}
}

void
DocCommentFinalizer::
removeInvalidReferences(Symbol& I)
{
    if (auto* asUsing = dynamic_cast<UsingSymbol*>(&I))
    {
        removeInvalidIds(corpus_, asUsing->ShadowDeclarations);
    }
    else if (auto* asNamespace = dynamic_cast<NamespaceSymbol*>(&I))
    {
        removeInvalidIds(corpus_, asNamespace->UsingDirectives);
    }
    else if (auto* asNamespaceAlias = dynamic_cast<NamespaceAliasSymbol*>(&I))
    {
        if (!corpus_.find(asNamespaceAlias->AliasedSymbol.id))
        {
            asNamespaceAlias->AliasedSymbol.id = SymbolID::invalid;
        }
    }
    else if (auto* asFunction = dynamic_cast<FunctionSymbol*>(&I))
    {
        if (asFunction->Template)
        {
            removeInvalidIds(corpus_, *asFunction->Template);
        }
    }
    else if (auto* asRecord = dynamic_cast<RecordSymbol*>(&I))
    {
        if (asRecord->Template)
        {
            removeInvalidIds(corpus_, *asRecord->Template);
        }
    }
    else if (auto* asTypedef = dynamic_cast<TypedefSymbol*>(&I))
    {
        if (asTypedef->Template)
        {
            removeInvalidIds(corpus_, *asTypedef->Template);
        }
    }
    else if (auto* asVariable = dynamic_cast<VariableSymbol*>(&I))
    {
        if (asVariable->Template)
        {
            removeInvalidIds(corpus_, *asVariable->Template);
        }
    }
    else if (auto* asConcept = dynamic_cast<ConceptSymbol*>(&I))
    {
        if (asConcept->Template)
        {
            removeInvalidIds(corpus_, *asConcept->Template);
        }
    }

    MRDOCS_CHECK_OR(I.doc);
    auto& J = *I.doc;
    removeInvalidReferences(J);
}

void
DocCommentFinalizer::
removeInvalidReferences(DocComment& doc)
{
    // Use the bottom up traversal to ensure that
    // we resolve references in inner nodes.
    // The only nodes that can contain references
    // are ReferenceInline and ThrowsBlock.
    bottomUpTraverse(doc, Overload(
        [this](DocComment& node) {
            removeInvalidIds(corpus_, node.relates);
            removeInvalidIds(corpus_, node.related);
        },
        [this](doc::ReferenceInline& node) {
            if (node.id != SymbolID::invalid)
            {
                if (!corpus_.find(node.id))
                {
                    node.id = SymbolID::invalid;
                }
            }
        },
        [this](doc::ThrowsBlock& node) {
            if (node.exception.id != SymbolID::invalid)
            {
                if (!corpus_.find(node.exception.id))
                {
                    node.exception.id = SymbolID::invalid;
                }
            }
        }));
}

namespace {
// Expand tabs to spaces using a tab stop of 8 (common in toolchains)
inline
std::string
expand_tabs(std::string_view s, std::size_t tabw)
{
    std::string out;
    out.reserve(s.size());
    std::size_t col = 0;
    for (char ch: s)
    {
        if (ch == '\t')
        {
            std::size_t spaces = tabw - (col % tabw);
            out.append(spaces, ' ');
            col += spaces;
        } else
        {
            out.push_back(ch);
            // naive column advance;
            // good enough for ASCII/byte-based columns
            ++col;
        }
    }
    return out;
}

// Split into lines; tolerates \n, \r\n, and final line w/o newline
inline
std::vector<std::string_view>
split_lines(std::string const& text)
{
    std::vector<std::string_view> lines;
    std::size_t start = 0;
    while (start <= text.size())
    {
        auto nl = text.find('\n', start);
        if (nl == std::string::npos)
        {
            // last line (may be empty)
            lines.emplace_back(text.data() + start, text.size() - start);
            break;
        }
        // trim a preceding '\r' if present
        std::size_t len = nl - start;
        if (len > 0 && text[nl - 1] == '\r')
        {
            --len;
        }
        lines.emplace_back(text.data() + start, len);
        start = nl + 1;
    }
    return lines;
}
} // namespace

void
DocCommentFinalizer::emitWarnings()
{
    MRDOCS_CHECK_OR(corpus_.config->warnings);
    warnUndocumented();
    warnDocErrors();
    warnNoParamDocs();
    warnUndocEnumValues();
    warnUnnamedParams();

    auto const level = !corpus_.config->warnAsError ?
                           report::Level::warn :
                           report::Level::error;

    // Simple cache for the last file we touched
    std::string_view lastPath;
    std::string fileContents;
    std::vector<std::string_view> fileLines;

    for (auto const& [loc, msgs]: warnings_)
    {
        // Build the location header
        std::string out;
        out += std::format("{}:{}:{}:\n", loc.FullPath, loc.LineNumber, loc.ColumnNumber);

        // Append grouped messages for this location
        {
            int i = 1;
            for (auto const& msg: msgs)
            {
                out += std::format("    {}) {}\n", i++, msg);
            }
        }

        // Render the source snippet if possible
        // Load file if path changed
        if (loc.FullPath != lastPath)
        {
            lastPath = loc.FullPath;
            fileContents.clear();
            fileLines.clear();

            if (auto expFileContents = files::getFileText(loc.FullPath);
                expFileContents)
            {
                fileContents = std::move(*expFileContents);
                fileLines = split_lines(fileContents);
            }
            else
            {
                fileLines.clear();
            }
        }

        if (loc.LineNumber < fileLines.size() &&
            loc.LineNumber > 0)
        {
            std::string_view rawLine = fileLines[loc.LineNumber - 1];
            std::size_t caretCol =
                loc.ColumnNumber < rawLine.size() &&
                loc.ColumnNumber > 0
                    ? loc.ColumnNumber - 1
                    : std::size_t(-1);
            std::string lineExpanded = expand_tabs(rawLine, 8);

            // Compute width for the line number gutter
            std::string gutter = std::format("  {} | ", loc.LineNumber);
            out += gutter;

            // Line text
            out += lineExpanded;
            out += "\n";

            // Create gutter for the caret line
            std::size_t const gutterWidth = gutter.size();
            gutter = std::string(gutterWidth - 2, ' ') + "| ";
            out += gutter;

            if (caretCol != std::size_t(-1) && caretCol < rawLine.size())
            {
                std::size_t expandedCaretCol = 0;
                for (std::size_t i = 0; i < caretCol; ++i)
                {
                    if (rawLine[i] == '\t')
                    {
                        expandedCaretCol += 8;
                    }
                    else
                    {
                        ++expandedCaretCol;
                    }
                }
                MRDOCS_ASSERT(expandedCaretCol <= lineExpanded.size());

                out += std::string(expandedCaretCol, ' ');
                out += "^";

                out += std::string(lineExpanded.size() - expandedCaretCol - 1, '~');
                out += "\n";
            }
        }

        report::log(level, out);
    }
}

void
DocCommentFinalizer::warnUndocumented()
{
    MRDOCS_CHECK_OR(corpus_.config->warnIfUndocumented);
    for (auto& undocI: corpus_.undocumented_)
    {
        if (Symbol const* I = corpus_.find(undocI.id))
        {
            MRDOCS_CHECK_OR(
                !I->doc || I->Extraction == ExtractionMode::Regular);
        }
        bool const prefer_definition = is_one_of(
            undocI.kind, {SymbolKind::Record, SymbolKind::Enum});
        this->warn(
            *getPrimaryLocation(undocI.Loc, prefer_definition),
            "{}: Symbol is undocumented",
            undocI.name);
    }
    corpus_.undocumented_.clear();
}

void
DocCommentFinalizer::
warnDocErrors()
{
    MRDOCS_CHECK_OR(corpus_.config->warnIfDocError);
    for (auto const& I : corpus_.info_)
    {
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(I->isFunction());
        warnParamErrors(dynamic_cast<FunctionSymbol const&>(*I));
    }
}

void
DocCommentFinalizer::
warnParamErrors(FunctionSymbol const& I)
{
    MRDOCS_CHECK_OR(I.doc);

    // Check for duplicate doc parameters
    auto docParamNames = getDocCommentParamNames(*I.doc);
    std::ranges::sort(docParamNames);
    auto [firstDup, lastUnique] = std::ranges::unique(docParamNames);
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
    docParamNames.erase(lastUnique, docParamNames.end());

    // Check for documented parameters that don't exist in the function
    auto paramNames =
        std::views::transform(I.Params, &Param::Name) |
        std::views::filter([](Optional<std::string> const& name) { return static_cast<bool>(name); }) |
        std::views::transform([](Optional<std::string> const& name) -> std::string_view { return *name; });
    for (std::string_view docParamName: docParamNames)
    {
        if (std::ranges::find(paramNames, docParamName) == paramNames.end())
        {
            this->warn(
                *getPrimaryLocation(I),
                "{}: Documented parameter '{}' does not exist",
                corpus_.Corpus::qualifiedName(I),
                docParamName);
        }
    }

}

void
DocCommentFinalizer::
warnNoParamDocs()
{
    MRDOCS_CHECK_OR(corpus_.config->warnNoParamdoc);
    for (auto const& I : corpus_.info_)
    {
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(I->isFunction());
        MRDOCS_CHECK_OR_CONTINUE(I->doc);
        warnNoParamDocs(dynamic_cast<FunctionSymbol const&>(*I));
    }
}

void
DocCommentFinalizer::
warnNoParamDocs(FunctionSymbol const& I)
{
    MRDOCS_CHECK_OR(!I.IsDeleted);
    // Check for function parameters that are not documented in doc
    auto docParamNames = getDocCommentParamNames(*I.doc);
    auto paramNames =
        std::views::transform(I.Params, &Param::Name) |
        std::views::filter([](Optional<std::string> const& name) { return name.has_value(); }) |
        std::views::transform([](Optional<std::string> const& name) -> std::string_view { return *name; }) |
        std::views::filter([](std::string_view const& name) { return !name.empty(); });
    for (auto const& paramName: paramNames)
    {
        if (std::ranges::find(docParamNames, paramName) == docParamNames.end())
        {
            this->warn(
                *getPrimaryLocation(I),
                "{}: Missing documentation for parameter '{}'",
                corpus_.Corpus::qualifiedName(I),
                paramName);
        }
    }

    // Check for undocumented return type
    if (I.doc->returns.empty())
    {
        MRDOCS_ASSERT(!I.ReturnType.valueless_after_move());
        auto isVoid = [](Type const& returnType) -> bool
        {
            if (returnType.isNamed())
            {
                auto const& namedReturnType = dynamic_cast<NamedType const&>(returnType);
                return namedReturnType.Name->Identifier == "void";
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
DocCommentFinalizer::
warnUndocEnumValues()
{
    MRDOCS_CHECK_OR(corpus_.config->warnIfUndocEnumVal);
    for (auto const& I : corpus_.info_)
    {
        MRDOCS_CHECK_OR_CONTINUE(I->isEnumConstant());
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(!I->doc);
        this->warn(
            *getPrimaryLocation(*I),
            "{}: Missing documentation for enum value",
            corpus_.Corpus::qualifiedName(*I));
    }
}

void
DocCommentFinalizer::
warnUnnamedParams()
{
    MRDOCS_CHECK_OR(corpus_.config->warnUnnamedParam);
    for (auto const& I : corpus_.info_)
    {
        MRDOCS_CHECK_OR_CONTINUE(I->isFunction());
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(I->doc);
        warnUnnamedParams(dynamic_cast<FunctionSymbol const&>(*I));
    }
}

void
DocCommentFinalizer::
warnUnnamedParams(FunctionSymbol const& I)
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

} // mrdocs
