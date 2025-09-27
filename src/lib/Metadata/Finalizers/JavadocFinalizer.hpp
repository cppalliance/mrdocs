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

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_JAVADOCFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_JAVADOCFINALIZER_HPP

#include <lib/CorpusImpl.hpp>
#include <lib/Metadata/InfoSet.hpp>
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Support/ScopeExit.hpp>
#include <format>
#include <set>
#include <utility>

namespace clang::mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid
    are not checked.
*/
class JavadocFinalizer
{
    CorpusImpl& corpus_;
    Info* current_context_ = nullptr;

    /* Broken references for which we have already emitted
       a warning.
     */
    std::set<std::pair<std::string, std::string>> refWarned_;

    /* Info objects that have been finalized

       This is used to avoid allow recursion when
       finalizing references.
     */
    std::set<Info const*> finalized_;

    /* Info objects whose brief have been finalized
     */
    std::set<Info const*> finalized_brief_;

    /* Info objects whose metadata have been finalized
     */
    std::set<Info const*> finalized_metadata_;

    // A comparison function that sorts locations by:
    // 1) ascending full path
    // 2) descending line number
    // This is the most convenient order to fix
    // warnings in the source code because fixing a problem
    // at a certain line will invalidate the line numbers
    // of all subsequent warnings.
    struct WarningLocationCompare
    {
        bool
        operator()(Location const& lhs, Location const& rhs) const
        {
            if (lhs.FullPath != rhs.FullPath)
            {
                return lhs.FullPath < rhs.FullPath;
            }
            if (lhs.LineNumber != rhs.LineNumber)
            {
                return lhs.LineNumber > rhs.LineNumber;
            }
            return lhs.ColumnNumber > rhs.ColumnNumber;
        }
    };

    /*  Warnings that should be emitted after finalization

        The warnings are initially stored in this container
        where the messages are sorted by location.

        This makes it easier for the user to go through
        the warnings in the order they appear in the source
        code and fix them.
     */
    std::map<Location, std::vector<std::string>, WarningLocationCompare> warnings_;

public:
    JavadocFinalizer(CorpusImpl& corpus)
        : corpus_(corpus)
    {
    }

    /** Finalize the javadoc for all symbols

        The procedure is composed of steps that resolve
        groups of javadoc components for each symbol.

        Groups of components processed later might depend
        on components processed earlier. For example, a
        function's javadoc might depend on the brief of
        the return type and the parameters. Or, the
        javadoc of an overload depends on the complete
        documentation of the functions it overloads.
     */
    void
    build();

private:
    /** Finalize the brief of a symbol

        The brief is the first paragraph of the documentation
        and is used in the index and in the documentation
        summary.
     */
    void
    finalizeBrief(Info& I);

    /** Copy the brief from another symbol

        This function copies the brief from another symbol
        to the current context. The brief is copied only if
        the brief of the current context is empty and
        it contains a reference to another symbol created
        with \@copybrief or \@copydoc.
     */
    void
    copyBrief(Javadoc& javadoc);

    /** Set brief content automatically

        If the brief is empty, this function sets the brief
        to the first paragraph of the documentation.
     */
    void
    setAutoBrief(Javadoc& javadoc) const;

    /** Finalize the metadata copies

        Copy the metadata from other symbols to the current
        context whenever the current context contains a
        reference to another symbol created with \@copydoc.
     */
    void
    finalizeMetadataCopies(Info& I);

    /** Populate function javadoc from with missing fields

        This function populates the function javadoc with
        missing fields of special functions.
     */
    void
    populateFunctionJavadoc(FunctionInfo&) const;

    /** Populate the metadata of overloads

        This function populates the metadata of overloads
        with the metadata of the functions it overloads.
     */
    void
    populateOverloadJavadoc(OverloadsInfo&);

    /** Resolve references in the javadoc

        This function resolves references in the javadoc
        of a symbol. The references are resolved by looking
        up the symbol in the corpus and setting the id of
        the reference.
     */
    void
    finalizeJavadoc(Info& I);

    /** Recursively finalize javadoc members

        This function also processes related symbols,
        merges consecutive blocks, trims blocks, and
        unindents code blocks.
     */
    void
    finalize(Javadoc& javadoc);

    /** Resolve \@relates references

        This function finds the ID of "relates" symbols
        and populates the "related" symbol with the inverse.
     */
    void
    processRelates(Javadoc& javadoc);

    /** Remove all temporary text nodes from a block

        The temporary nodes (copied, related, etc...) should
        have been processed by the previous steps and should
        not be present in the final output.
     */
    void
    removeTempTextNodes(Javadoc& javadoc);

    /// A range of values derived from blocks
    template <std::ranges::range R>
    requires std::derived_from<std::ranges::range_value_t<R>, doc::Block>
    void
    removeTempTextNodes(R&& blocks)
    {
        for (auto& block: blocks)
        {
            removeTempTextNodes(block);
        }
    }

    /// Remove temporary text nodes from a block
    void
    removeTempTextNodes(std::vector<Polymorphic<doc::Block>>& blocks);

    /// Remove temporary text nodes from a block
    void
    removeTempTextNodes(doc::Block& block);

    /** Trim all block childen in the javadoc

        The first child is rtrimmed and the last child is ltrimmed.

        Like in HTML, multiple whitespaces (spaces, tabs, and newlines)
        between and within child nodes are collapsed into a single space.
     */
    void
    trimBlocks(Javadoc& javadoc);

    /// A range of values derived from blocks
    template <std::ranges::range R>
    requires std::derived_from<std::ranges::range_value_t<R>, doc::Block>
    void
    trimBlocks(R&& blocks)
    {
        for (auto& block: blocks)
        {
            trimBlock(block);
        }
    }

    /// Trim a block
    void
    trimBlocks(std::vector<Polymorphic<doc::Block>>& blocks);

    /// Trim a block
    void
    trimBlock(doc::Block& block);

    /// Unindent all code blocks in the javadoc
    void
    unindentCodeBlocks(Javadoc& javadoc);

    /// Unindent code blocks in a range
    template <std::ranges::range R>
    requires std::derived_from<std::ranges::range_value_t<R>, doc::Block>
    void
    unindentCodeBlocks(R&& blocks)
    {
        for (auto& block: blocks)
        {
            unindentCodeBlocks(block);
        }
    }

    /// Unindent code blocks in a vector
    void
    unindentCodeBlocks(std::vector<Polymorphic<doc::Block>>& blocks);

    /// Unindent a code block
    void
    unindentCodeBlocks(doc::Block& block);

    /** Check and finalize data unrelated to javadoc

        This checks if all symbol IDs are valid and
        removes invalid IDs from the Info data.
     */
    template <class InfoTy>
    void
    finalizeInfoData(InfoTy& I);

    /** Check the documentation for problems and creates warnings
     */
    void
    emitWarnings();

    // Look for symbol and set the id of a reference
    void
    finalize(doc::Reference& ref, bool emitWarning = true);

    // Recursively finalize references in a javadoc node
    void
    finalize(doc::Node& node);

    // Automatically find related symbols
    void
    setAutoRelates();

    // Copy brief and details to the current context
    void
    copyDetails(Javadoc& javadoc);

    // Set id to invalid if it does not exist
    void
    finalize(SymbolID& id);

    // Remove invalid ids from a vector
    void
    finalize(std::vector<SymbolID>& ids);

    // Remove invalid ids from TArg members
    void
    finalize(TArg& arg);

    // Remove invalid ids from TParam members
    void
    finalize(TParam& param);

    // Remove invalid ids from Javadoc members
    void
    finalize(Param& param);

    /// Remove invalid ids from BaseInfo members
    void
    finalize(BaseInfo& info);

    /// Remove invalid ids from TemplateInfo members
    void
    finalize(TemplateInfo& info);

    /// Remove invalid ids from TypeInfo members
    void
    finalize(TypeInfo& type);

    /// Remove invalid ids from NameInfo members
    void
    finalize(NameInfo& name);

    /// Finalize polymorphic
    template <class T>
    void
    finalize(Polymorphic<T>&& val)
    {
        MRDOCS_ASSERT(!val.valueless_after_move());
        finalize(*val);
    }

    template <class T>
    void
    finalize(Polymorphic<T>& val)
    {
        MRDOCS_ASSERT(!val.valueless_after_move());
        finalize(*val);
    }

    /// Finalize optional and pointer-like members
    template <dereferenceable T>
    void
    finalize(T&& val) requires
    requires { this->finalize(*val); }
    {
        if (val)
        {
            finalize(*val);
        }
    }

    /// Finalize a range of elements
    template<typename Range>
    requires std::ranges::input_range<Range>
    void
    finalize(Range&& range)
    {
        for (auto&& elem: range)
        {
            finalize(elem);
        }
    }

    // Check if SymbolID exists in info
    void
    checkExists(SymbolID const& id) const;

    // Check if SymbolIDs exist in info
    template <range_of<SymbolID> T>
    void
    checkExists(T&& ids) const
    {
        MRDOCS_ASSERT(std::ranges::all_of(ids,
            [this](SymbolID const& id)
            {
                return corpus_.find(id) != nullptr;
            }));
    }



    template<class... Args>
    void
    warn(
        Location const& loc,
        report::Located<std::string_view> const format,
        Args&&... args)
    {
        MRDOCS_CHECK_OR(corpus_.config->warnings);
        std::string const str =
            std::vformat(format.value, std::make_format_args(args...));
        warnings_[loc].push_back(str);
    }

    template<class... Args>
    void
    warn(
        report::Located<std::string_view> const format,
        Args&&... args)
    {
        MRDOCS_CHECK_OR(corpus_.config->warnings);
        MRDOCS_ASSERT(current_context_);
        auto loc = getPrimaryLocation(*current_context_);
        warn(*loc, format, std::forward<Args>(args)...);
    }

    void
    warnUndocumented();

    void
    warnDocErrors();

    void
    warnParamErrors(FunctionInfo const& I);

    void
    warnNoParamDocs();

    void
    warnNoParamDocs(FunctionInfo const& I);

    void
    warnUndocEnumValues();

    void
    warnUnnamedParams();

    void
    warnUnnamedParams(FunctionInfo const& I);
};

} // clang::mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_JAVADOCFINALIZER_HPP
