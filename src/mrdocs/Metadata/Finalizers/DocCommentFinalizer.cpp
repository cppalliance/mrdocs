//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "DocCommentFinalizer.hpp"
#include "DocComment/Function.hpp"
#include "DocComment/Overloads.hpp"
#include "DocComment/parseInlines.hpp"
#include <mrdocs/ADT/Overload.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Support/Container/Algorithm.hpp>
#include <mrdocs/Support/Filesystem/Path.hpp>
#include <mrdocs/Support/ScopeExit.hpp>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallVector.h>
#include <algorithm>
#include <format>
#include <unordered_map>

namespace mrdocs {

namespace {

using InlineIter = std::vector<Polymorphic<doc::Inline>>::iterator;
using BlockVec   = std::vector<Polymorphic<doc::Block>>;
using BlockIter  = BlockVec::iterator;

// Check if an inline starts with a Markdown "- " list marker.
bool
isMarkdownListMarker(doc::Inline const& inl)
{
    if (!inl.isText())
    {
        return false;
    }
    std::string_view const sv = ltrim(inl.asText().literal);
    return sv.starts_with("- ");
}

// Return the text content after stripping the "- " prefix.
std::string
textAfterListMarker(std::string_view literal)
{
    std::string_view const sv = ltrim(literal);
    MRDOCS_ASSERT(sv.starts_with("- "));
    return std::string(sv.substr(2));
}

// Move non-marker continuation inlines into a paragraph.
void
gatherContinuation(
    doc::ParagraphBlock& para,
    InlineIter& it,
    InlineIter end)
{
    while (it != end && !isMarkdownListMarker(**it))
    {
        para.children.push_back(std::move(*it));
        ++it;
    }
}

// Build one ListItem from a "- " marker and its continuation.
doc::ListItem
buildListItem(InlineIter& it, InlineIter end)
{
    doc::ParagraphBlock para;
    std::string const content = textAfterListMarker(
        (*it)->asText().literal);
    if (!content.empty())
    {
        para.emplace_back<doc::TextInline>(content);
    }
    ++it;
    gatherContinuation(para, it, end);
    doc::ListItem item;
    item.blocks.emplace_back(std::move(para));
    return item;
}

// Build a ListBlock from all "- " items in [first, last).
doc::ListBlock
buildListFromRange(InlineIter first, InlineIter last)
{
    doc::ListBlock list;
    while (first != last)
    {
        MRDOCS_ASSERT(isMarkdownListMarker(**first));
        list.items.push_back(buildListItem(first, last));
    }
    return list;
}

// Find the first "- " marker among a paragraph's children.
InlineIter
findFirstMarker(doc::ParagraphBlock& para)
{
    return std::find_if(
        para.children.begin(),
        para.children.end(),
        [](Polymorphic<doc::Inline> const& el) {
            return isMarkdownListMarker(*el);
        });
}

// Trim trailing whitespace and remove empty inlines.
void
cleanupParagraph(doc::ParagraphBlock& para)
{
    doc::rtrim(para.asInlineContainer());
    std::erase_if(
        para.children,
        [](Polymorphic<doc::Inline> const& el) {
            return doc::isEmpty(el);
        });
}

// Extract a ListBlock from a paragraph's "- " markers.
// Returns std::nullopt if no markers are found.
Optional<doc::ListBlock>
extractList(doc::ParagraphBlock& para)
{
    InlineIter const marker = findFirstMarker(para);
    if (marker == para.children.end())
    {
        return std::nullopt;
    }
    doc::ListBlock list = buildListFromRange(
        marker, para.children.end());
    para.children.erase(marker, para.children.end());
    cleanupParagraph(para);
    return list;
}

// Insert a list block, replacing or following the paragraph.
// Returns an iterator past the inserted list.
BlockIter
insertList(BlockVec& blocks, BlockIter it, doc::ListBlock&& list)
{
    if ((*it)->asParagraph().empty())
    {
        it = blocks.erase(it);
    }
    else
    {
        ++it;
    }
    it = blocks.emplace(it, std::move(list));
    return ++it;
}

// Scan paragraphs in a block vector and split any that
// contain "- " markers into a prefix paragraph and a ListBlock.
void
splitParagraphsAtMarkers(BlockVec& blocks)
{
    for (BlockIter it = blocks.begin(); it != blocks.end(); )
    {
        if (!(*it)->isParagraph())
        {
            ++it;
            continue;
        }
        Optional<doc::ListBlock> list = extractList(
            (*it)->asParagraph());
        if (!list)
        {
            ++it;
            continue;
        }
        it = insertList(blocks, it, std::move(*list));
    }
}

// Convert a paragraph whose whole text is a standalone display-math span
// ($$ ... $$) into a MathBlock, so display math renders as a block instead
// of an inline formula. Inline $...$ or $$...$$ mixed with other text is
// left untouched for parseInlinesInContainer to turn into a MathInline.
// Runs before inline parsing, while the paragraph still holds raw text.
void
promoteDisplayMathParagraphs(BlockVec& blocks)
{
    for (auto& block : blocks)
    {
        if (!block->isParagraph())
        {
            continue;
        }
        doc::ParagraphBlock& para = block->asParagraph();
        std::string text;
        bool pureText = true;
        for (auto const& child : para.children)
        {
            if (child->isText())
            {
                text += child->asText().literal;
            }
            else if (child->isSoftBreak() || child->isLineBreak())
            {
                text += ' ';
            }
            else
            {
                pureText = false;
                break;
            }
        }
        if (!pureText)
        {
            continue;
        }
        std::string_view const sv = trim(text);
        if (sv.size() < 5
            || !sv.starts_with("$$")
            || !sv.ends_with("$$"))
        {
            continue;
        }
        std::string_view const inner = sv.substr(2, sv.size() - 4);
        // Two spans on one line are two inline formulas, not one block.
        if (inner.find("$$") != std::string_view::npos)
        {
            continue;
        }
        doc::MathBlock math;
        math.literal = std::string(trim(inner));
        block = Polymorphic<doc::Block>(std::move(math));
    }
}

// Convert a paragraph that opens with a Markdown footnote definition
// (`[^label]: text`) into a FootnoteDefinitionBlock holding the text. The
// matching `[^label]` reference is produced by the inline parser. Runs on
// raw text, before inline parsing, so the definition body is inline-parsed
// later when the traversal descends into the new block.
void
extractFootnoteDefinitions(BlockVec& blocks)
{
    for (auto& block : blocks)
    {
        if (!block->isParagraph())
        {
            continue;
        }
        doc::ParagraphBlock& para = block->asParagraph();
        if (para.children.empty() || !para.children.front()->isText())
        {
            continue;
        }
        std::string_view const head = ltrim(para.children.front()->asText().literal);
        if (!head.starts_with("[^"))
        {
            continue;
        }
        std::size_t const close = head.find("]:", 2);
        if (close == std::string_view::npos || close == 2)
        {
            continue;
        }
        std::string label(head.substr(2, close - 2));
        std::string rest(ltrim(head.substr(close + 2)));
        para.children.front()->asText().literal = std::move(rest);

        doc::FootnoteDefinitionBlock def;
        def.label = std::move(label);
        def.blocks.emplace_back(
            Polymorphic<doc::Block>(std::move(para)));
        block = Polymorphic<doc::Block>(std::move(def));
    }
}

// Move footnote definitions out of the document flow into the DocComment's
// floating `footnotes` list. They are written inline (as a `[^label]: text`
// paragraph) but belong to the whole comment, and are rendered together in a
// footnotes section at the end of the page instead of where they appear.
void
hoistFootnoteDefinitions(DocComment& doc)
{
    for (auto it = doc.Document.begin(); it != doc.Document.end();)
    {
        if ((*it)->isFootnoteDefinition())
        {
            doc.footnotes.push_back(std::move((*it)->asFootnoteDefinition()));
            it = doc.Document.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// Parse inline Markdown (bold, italic, code, etc.) in a
// single InlineContainer's immediate text children.
// Does *not* recurse: topDownTraverse() already visits every
// InlineContainer in the tree, so each node is processed
// exactly once.
void
parseInlinesInContainer(doc::InlineContainer& node)
{
    // Reserve enough capacity so child inserts during parsing
    // cannot reallocate and invalidate pointers held by the
    // parser state.
    std::size_t extra = 0;
    for (auto const& child : node.children)
    {
        if (child->isText())
        {
            extra += child->asText().literal.size();
        }
    }
    if (extra > 0)
    {
        node.children.reserve(
            node.children.size() + 2 * extra + 16);
    }

    auto it = node.children.begin();
    while (it != node.children.end())
    {
        Polymorphic<doc::Inline>& el = *it;

        if (!el->isText())
        {
            ++it;
            continue;
        }

        auto& textEl = el->asText();
        doc::InlineContainer v;
        ParseResult r = parse(textEl.literal, v);

        if (!r)
        {
            ++it;
            continue;
        }

        it = node.children.erase(it);

        for (auto& child : v.children)
        {
            it = node.children.insert(it, std::move(child));
            ++it;
        }
    }
}

}

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
    if (config_.overloads)
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
    if (config_.autoFunctionMetadata)
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

    // Parse inlines in terminal text nodes
    for (auto& I : infos)
    {
        parseInlines(I);
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
        populateOverloadsBrief(OI, functions, config_);
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
            if (config_.warnings &&
                config_.warnBrokenRef &&
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
            if (config_.warnings &&
                config_.warnBrokenRef &&
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
    MRDOCS_CHECK_OR(config_.autoBrief);
    MRDOCS_CHECK_OR(!doc.brief);
    MRDOCS_CHECK_OR(!doc.Document.empty());

    // A paragraph can serve as the brief when it has visible content of its
    // own. Plain text counts unless it is empty or whitespace; so does any
    // styled span, code, link, reference, or formula: `\c char8_t.` is a
    // fine brief even though none of it is plain text. Breaks carry nothing,
    // and a @copydetails placeholder is resolved later into someone else's
    // description, so neither may be promoted.
    // See tests/golden/fixtures/config/auto-brief/inline-command-brief.cpp
    auto isInvalidBriefText = [](
        Polymorphic<doc::Inline> const& el)
        {
            MRDOCS_ASSERT(!el.valueless_after_move());
            if (el->isText())
            {
                return el->asText().literal.empty() ||
                       isWhitespace(el->asText().literal);
            }
            return el->Kind == doc::InlineKind::SoftBreak ||
                   el->Kind == doc::InlineKind::LineBreak ||
                   el->Kind == doc::InlineKind::CopyDetails;
        };

    for (auto it = doc.Document.begin(); it != doc.Document.end();)
    {
        // A heading marks the start of a section (as of writing
        // this, the only way to produce one is an @par with an
        // explicit title). Anything that follows is the body of
        // that section, not the symbol's introductory prose, so
        // stop looking for a brief here. See issue #1162.
        if ((*it)->Kind == doc::BlockKind::Heading)
        {
            return;
        }
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

#include "DocCommentFinalizer/CopyDetails.ipp"


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
        config_.warnings &&
        config_.warnBrokenRef &&
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

        if (!is_one_of(I.FuncClass, {
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
    if (config_.autoRelates)
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
    Corpus& corpus)
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
        MRDOCS_CHECK_OR(!type.valueless_after_move(), nullptr);
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

    // Remove duplicates from relatedRecordsOrEnums.
    //
    // Use plain std::sort/std::unique here instead of the ranges
    // versions: libstdc++-15's `ranges::less` probes `operator<=>`
    // via ADL on the element type, which on `Symbol*` reaches our
    // generic mrdocs::operator<=> template.  Clang 19 hard-errors
    // when substituting T = Symbol* into that template (operator<=>
    // requires a class/enum parameter), instead of SFINAE'ing the
    // candidate out of overload resolution -- a regression that's
    // present in 19 but absent in 18 and >=20.
    std::sort(
        relatedRecordsOrEnums.begin(), relatedRecordsOrEnums.end());
    relatedRecordsOrEnums.erase(
        std::unique(
            relatedRecordsOrEnums.begin(), relatedRecordsOrEnums.end()),
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
    // Single top-down traversal of the entire DocComment tree.
    //
    // At each node the visitor does two things in order:
    //   (1) Split paragraphs at "- " markers (block restructure).
    //   (2) Parse inline Markdown (bold, italic, code, etc.).
    //
    // Top-down ordering is essential: list splitting must run
    // on raw text *before* inline parsing, because parse() merges
    // text across line boundaries, burying "- " markers inside
    // TextInline nodes where isMarkdownListMarker() cannot find
    // them.
    //
    // topDownTraverse() calls the visitor before iterating
    // children, so the range-for loops see the already-modified
    // vectors. This is safe because the visitor returns before
    // the loops capture begin()/end().
    topDownTraverse(doc, []<class NodeTy>(NodeTy& node) {
        // (1) Block restructure: split paragraphs containing
        //     "- " markers into a prefix paragraph + ListBlock.
        if constexpr (
            requires {
                { node.Document } ->
                    range_of<Polymorphic<doc::Block>>;
            })
        {
            splitParagraphsAtMarkers(node.Document);
            promoteDisplayMathParagraphs(node.Document);
            extractFootnoteDefinitions(node.Document);
        }
        if constexpr (
            requires {
                { node.blocks } ->
                    range_of<Polymorphic<doc::Block>>;
            })
        {
            splitParagraphsAtMarkers(node.blocks);
            promoteDisplayMathParagraphs(node.blocks);
            extractFootnoteDefinitions(node.blocks);
        }
        if constexpr (std::same_as<NodeTy, doc::ListBlock>)
        {
            for (doc::ListItem& item : node.items)
            {
                splitParagraphsAtMarkers(item.blocks);
                promoteDisplayMathParagraphs(item.blocks);
                extractFootnoteDefinitions(item.blocks);
            }
        }

        // (2) Inline parsing: parse bold, italic, code,
        //     links, etc. in text nodes.
        if constexpr (
            std::derived_from<NodeTy, doc::InlineContainer>)
        {
            parseInlinesInContainer(node);
        }
    });

    // Footnote definitions are parsed inline above; collect them into the
    // floating `footnotes` list so they render once at the end of the page.
    hoistFootnoteDefinitions(doc);
}

namespace {
// A function erases all references in a vector that don't exist
// in the corpus with invalid references.
inline void
removeInvalidIds(Corpus& corpus, std::vector<SymbolID>& refs)
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
removeInvalidIds(Corpus& corpus, std::vector<struct Name>& refs)
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
removeInvalidIds(Corpus& corpus, std::vector<doc::ReferenceInline>& refs)
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
removeInvalidIds(Corpus& corpus, TemplateInfo& T)
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
    MRDOCS_CHECK_OR(config_.warnings);
    warnUndocumented();
    warnNoBrief();
    warnDocErrors();
    warnNoParamDocs();
    warnUnnamedParams();

    auto const level = !config_.warnAsError ?
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

    if (warningLimitReached_ && config_.warnAsError && config_.maxErrors > 0)
    {
        report::log(
            level,
            std::format(
                "error limit reached (max-errors={}); further diagnostics "
                "suppressed. Rerun with --max-errors=0 for the full report.\n",
                config_.maxErrors));
    }
}

void
DocCommentFinalizer::warnUndocumented()
{
    MRDOCS_CHECK_OR(config_.warnIfUndocumented || config_.warnIfUndocEnumVal);
    // A template specialization that is folded onto its primary (and every
    // member it carries) borrows the primary's documentation, so it never needs
    // its own. This holds for the symbol itself or any enclosing scope.
    auto isListedOnPrimary = [](Symbol const& S)
    {
        if (auto const* r = S.asRecordPtr())
        {
            return r->IsListedOnPrimary;
        }
        if (auto const* f = S.asFunctionPtr())
        {
            return f->IsListedOnPrimary;
        }
        return false;
    };
    auto inFoldedSpecialization = [&](Symbol const& S)
    {
        for (Symbol const* cur = &S; cur;)
        {
            if (isListedOnPrimary(*cur))
            {
                return true;
            }
            cur = cur->Parent ? corpus_.find(cur->Parent) : nullptr;
        }
        return false;
    };
    for (auto const& undocI: corpus_.undocumented_)
    {
        if (!warningBudgetRemaining())
        {
            warningLimitReached_ = true;
            break;
        }
        Symbol const* const I = corpus_.find(undocI.id);
        if (I)
        {
            // a symbol that gained documentation from a redeclaration
            MRDOCS_CHECK_OR_CONTINUE(!I->doc);
            // a symbol that was turned into an inherited copy
            MRDOCS_CHECK_OR_CONTINUE(!I->IsCopyFromInherited);
            // a symbol that was folded onto a template specialization's
            // primary and so borrows the primary's documentation
            MRDOCS_CHECK_OR_CONTINUE(!inFoldedSpecialization(*I));
        }
        // Enum constants carry the dedicated enum-value wording. The set only
        // holds them when warn-if-undoc-enum-val is enabled (checkUndocumented).
        if (undocI.kind == SymbolKind::EnumConstant)
        {
            this->warn(
                *getPrimaryLocation(undocI.Loc, false),
                "{}: Missing documentation for enum value",
                I ? corpus_.Corpus::qualifiedName(*I) : undocI.name);
            continue;
        }
        bool const prefer_definition = is_one_of(
            undocI.kind, {SymbolKind::Record, SymbolKind::Enum});
        this->warn(
            *getPrimaryLocation(undocI.Loc, prefer_definition),
            "{}: {} is undocumented",
            undocI.name,
            toString(undocI.kind));
    }
    corpus_.undocumented_.clear();
}

void
DocCommentFinalizer::
warnNoBrief()
{
    MRDOCS_CHECK_OR(config_.warnNoBrief);
    for (auto const& I : corpus_.info_)
    {
        if (!warningBudgetRemaining())
        {
            warningLimitReached_ = true;
            break;
        }
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(I->IsCopyFromInherited == false);
        // Overload sets synthesize their page from members and never carry a
        // brief of their own, so they are exempt.
        MRDOCS_CHECK_OR_CONTINUE(!I->isOverloads());
        // A symbol with no documentation at all is reported by
        // warnUndocumented; here we only flag symbols that carry some
        // documentation but, after brief finalization (auto-brief and
        // @copybrief), still have no brief. Their summary cell in the parent's
        // member table renders empty, so treat the missing brief as missing
        // documentation.
        MRDOCS_CHECK_OR_CONTINUE(I->doc);
        MRDOCS_CHECK_OR_CONTINUE(!I->doc->brief);
        // The `related` list is populated from *other* symbols that `@relates`
        // to this one; it is not documentation the author wrote for this
        // symbol. A doc whose only content is such a back-populated `related`
        // entry (no prose, brief, params, returns, ...) is effectively
        // undocumented, so reporting a missing brief for it is a false
        // positive (this happens when a free function relates to a class or
        // template specialization that itself carries no authored comment).
        {
            auto const& d = *I->doc;
            bool const hasAuthoredContent =
                !d.Document.empty() ||
                !d.params.empty() ||
                !d.tparams.empty() ||
                !d.returns.empty() ||
                !d.exceptions.empty() ||
                !d.sees.empty() ||
                !d.preconditions.empty() ||
                !d.postconditions.empty() ||
                !d.footnotes.empty();
            MRDOCS_CHECK_OR_CONTINUE(hasAuthoredContent);
        }
        auto const loc = getPrimaryLocation(*I);
        MRDOCS_CHECK_OR_CONTINUE(loc);
        this->warn(
            *loc,
            "{}: symbol is documented but has no brief",
            corpus_.Corpus::qualifiedName(*I));
    }
}

void
DocCommentFinalizer::
warnDocErrors()
{
    MRDOCS_CHECK_OR(config_.warnIfDocError);
    for (auto const& I : corpus_.info_)
    {
        if (!warningBudgetRemaining())
        {
            warningLimitReached_ = true;
            break;
        }
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(I->IsCopyFromInherited == false);
        if (I->isFunction())
        {
            warnParamErrors(dynamic_cast<FunctionSymbol const&>(*I));
        }
        else if (I->isMacro())
        {
            warnParamErrors(dynamic_cast<MacroSymbol const&>(*I));
        }
    }
}

std::vector<std::string_view>
DocCommentFinalizer::
siblingParamNames(FunctionSymbol const& I) const
{
    std::vector<std::string_view> names;
    auto const loc = getPrimaryLocation(I);
    MRDOCS_CHECK_OR(loc, names);
    auto const key = [](Location const& L)
    {
        return std::format("{}:{}:{}", L.FullPath, L.LineNumber, L.ColumnNumber);
    };
    // Built on the first request only. A documented parameter that no
    // parameter matches is rare, so most runs never pay for the grouping.
    // Functions from one macro expansion share the file, line, and column
    // of the invocation; the column keeps two functions written by hand on
    // one line apart.
    if (!functionsByLocation_)
    {
        functionsByLocation_.emplace();
        for (auto const& other : corpus_.info_)
        {
            MRDOCS_CHECK_OR_CONTINUE(other->isFunction());
            auto const otherLoc = getPrimaryLocation(*other);
            MRDOCS_CHECK_OR_CONTINUE(otherLoc);
            (*functionsByLocation_)[key(*otherLoc)].push_back(
                &dynamic_cast<FunctionSymbol const&>(*other));
        }
    }
    auto const it = functionsByLocation_->find(key(*loc));
    MRDOCS_CHECK_OR(it != functionsByLocation_->end(), names);
    for (FunctionSymbol const* other : it->second)
    {
        MRDOCS_CHECK_OR_CONTINUE(other != &I);
        for (Param const& P : other->Params)
        {
            if (P.Name && !P.Name->empty())
            {
                names.push_back(*P.Name);
            }
        }
    }
    return names;
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

    // Check for documented parameters that don't exist in the function.
    // Functions declared on the same line come from one macro expansion and
    // share the comment above it, so a parameter any of them has is valid.
    // See tests/golden/fixtures/symbols/function/macro-shared-comment.cpp
    auto paramNames =
        std::views::transform(I.Params, &Param::Name) |
        std::views::filter([](Optional<std::string> const& name) { return static_cast<bool>(name); }) |
        std::views::transform([](Optional<std::string> const& name) -> std::string_view { return *name; });
    std::vector<std::string_view> siblingNames;
    bool siblingsKnown = false;
    for (std::string_view docParamName: docParamNames)
    {
        MRDOCS_CHECK_OR_CONTINUE(
            std::ranges::find(paramNames, docParamName) == paramNames.end());
        if (!siblingsKnown)
        {
            siblingNames = siblingParamNames(I);
            siblingsKnown = true;
        }
        if (std::ranges::find(siblingNames, docParamName) == siblingNames.end())
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
warnParamErrors(MacroSymbol const& I)
{
    MRDOCS_CHECK_OR(I.doc);

    auto docParamNames = getDocCommentParamNames(*I.doc);

    // Check for duplicate doc parameters
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

    // Check for documented parameters that don't exist on the macro.
    // For variadic macros, accept both `...` and `__VA_ARGS__` as valid
    // names for the variadic argument list.
    auto isMacroParam = [&](std::string_view const name)
    {
        if (std::ranges::find(I.Parameters, name) != I.Parameters.end())
        {
            return true;
        }
        return I.IsVariadic && (name == "..." || name == "__VA_ARGS__");
    };
    for (std::string_view docParamName: docParamNames)
    {
        if (!isMacroParam(docParamName))
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
    MRDOCS_CHECK_OR(config_.warnNoParamdoc);
    for (auto const& I : corpus_.info_)
    {
        if (!warningBudgetRemaining())
        {
            warningLimitReached_ = true;
            break;
        }
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(I->IsCopyFromInherited == false);
        MRDOCS_CHECK_OR_CONTINUE(I->doc);
        if (I->isFunction())
        {
            warnNoParamDocs(dynamic_cast<FunctionSymbol const&>(*I));
        }
        else if (I->isMacro())
        {
            warnNoParamDocs(dynamic_cast<MacroSymbol const&>(*I));
        }
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
warnNoParamDocs(MacroSymbol const& I)
{
    // Only the named parameters are required to be
    // documented. The variadic argument list is optional;
    // if the user does document it (as `@param ...` or
    // `@param __VA_ARGS__`), the chosen name's validity
    // is checked in `warnParamErrors`.
    auto docParamNames = getDocCommentParamNames(*I.doc);
    for (std::string_view paramName : I.Parameters)
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
}

void
DocCommentFinalizer::
warnUnnamedParams()
{
    MRDOCS_CHECK_OR(config_.warnUnnamedParam);
    for (auto const& I : corpus_.info_)
    {
        if (!warningBudgetRemaining())
        {
            warningLimitReached_ = true;
            break;
        }
        MRDOCS_CHECK_OR_CONTINUE(I->isFunction());
        MRDOCS_CHECK_OR_CONTINUE(I->Extraction == ExtractionMode::Regular);
        MRDOCS_CHECK_OR_CONTINUE(I->IsCopyFromInherited == false);
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
