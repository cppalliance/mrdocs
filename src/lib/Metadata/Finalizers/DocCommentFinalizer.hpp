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

#ifndef MRDOCS_LIB_METADATA_FINALIZERS_DOCCOMMENTFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZERS_DOCCOMMENTFINALIZER_HPP

#include <lib/CorpusImpl.hpp>
#include <lib/Metadata/SymbolSet.hpp>
#include <mrdocs/Support/Report.hpp>
#include <mrdocs/Support/ScopeExit.hpp>
#include <format>
#include <set>
#include <utility>

namespace mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid
    are not checked.
*/
class DocCommentFinalizer {
    CorpusImpl& corpus_;

    /* Broken references for which we have already emitted
       a warning.
     */
    std::set<std::pair<std::string, std::string>> refWarned_;

    /* Info objects whose briefs have been finalized
     */
    std::set<Symbol const*> finalized_brief_;

    /* Info objects whose metadata has been finalized
     */
    std::set<Symbol const*> finalized_metadata_;

    /* Info objects that have been finalized

        This is used to avoid recursion when finalizing
        references.
     */
    std::set<Symbol const*> finalized_;

    // A comparison function that sorts locations by:
    // 1) ascending full path
    // 2) descending line number
    // This is the most convenient order for users to fix
    // warnings in the source code. This is because fixing a problem
    // at a particular line, without this ordering, would invalidate
    // the line numbers of all subsequent warnings.
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
    DocCommentFinalizer(CorpusImpl& corpus)
        : corpus_(corpus)
    {
    }

    /** Finalize the doc for all symbols
     */
    void
    build();

private:
    /*  Finalize the brief of a symbol

        This might mean copying the brief from another
        symbol (when there's a copybrief command) or
        populating it automatically (first sentence).
     */
    void
    finalizeBrief(Symbol& I);

    void
    copyBrief(Symbol const& ctx, DocComment& doc);

    void
    setAutoBrief(DocComment& doc) const;

    /*  Finalize the metadata copies

        Copy the details and metadata from other symbols to
        the current symbol context whenever the current
        context contains a reference to another symbol
        created with \@copydoc or \@copydetails.
     */
    void
    copyDetails(Symbol& I);

    void
    copyDetails(Symbol const& ctx, DocComment& doc);

    /*  Populate the metadata of overloads

        This function populates the metadata of overloads
        with the metadata of the functions it overloads.
     */
    void
    generateOverloadDocs(OverloadsSymbol&);

    /*  Resolve references in the doc

        This function traverses the doc tree
        of a symbol and resolves all references
        to other symbols.

        The references are resolved by looking
        up the symbol in the corpus and setting the ID of
        the reference.
     */
    void
    resolveReferences(Symbol& I);

    void
    resolveReference(
        Symbol const& I,
        doc::ReferenceInline& ref,
        bool emitWarning);

    /*  Populate function doc from with missing fields

        This function populates the function doc with
        missing fields of special functions.
     */
    void
    generateAutoFunctionMetadata(FunctionSymbol&) const;

    /*  Populate and resolve \@relates references

        This function populates the "relates" symbols
        of a doc (if the option is enabled), then
        finds the related symbols, resolves them.
        In other words, it also sets the inverse
        of the "relates" reference so that the related
        symbol also knows about the function that
        relates to it and can generate a link to it
        in the non-member functions section.
     */
    void
    processRelates(Symbol& I)
    {
        MRDOCS_CHECK_OR(I.doc);
        processRelates(I, *I.doc);
    }

    void
    processRelates(Symbol& I, DocComment& doc);


    void
    setAutoRelates(Symbol& I);

    /* Normalize doc siblings

      We first do a post-order structural merge/flatten so that, by the time we
      run the tidy-up pass below, each container's children are already in a
      canonical form:

      - adjacent Text nodes are coalesced,
      - adjacent wrappers of the same kind/attributes are merged,
      - trivial same-type nesting is flattened.

      Doing this in a dedicated pass avoids backtracking and iterator
      invalidation in the tidy-up phase, and guarantees that the tidy-up can
      make a single linear scan over each child list without missing newly
      created adjacencies.
    */
    void
    normalizeSiblings(DocComment& doc);

    void
    normalizeSiblings(Symbol& I)
    {
        MRDOCS_CHECK_OR(I.doc);
        normalizeSiblings(*I.doc);
    }

    /* Tidy up the doc:

       This function performs various bottom-up
       tidying operations on the doc, such as:

       - Remove any @copy* nodes that got left behind
       - Trimming leading and trailing empty inlines
       - Merging consecutive empty blocks (like HTML whitespace normalization)
       - Remove any blocks or inlines without content
         (especially after we do the trimming bottom up)
       - Unindenting code blocks.
    */
    void
    tidyUp(DocComment& doc);

    void
    tidyUp(Symbol& I)
    {
        MRDOCS_CHECK_OR(I.doc);
        tidyUp(*I.doc);
        if (I.doc->empty())
        {
            I.doc.reset();
        }
    }

    /* Parse inlines in terminal text nodes.
    */
    void
    parseInlines(DocComment& doc);

    void
    parseInlines(Symbol& I)
    {
        MRDOCS_CHECK_OR(I.doc);
        parseInlines(*I.doc);
    }

    /* Remove references to symbols that are not in the corpus

       This function traverses the symbol and DocComment
       tree of a symbol and removes all references
       to symbols that do not exist in the corpus.

       These are references clang was able to resolve
       when generating the AST, but which do not exist
       in the final MrDocs corpus, so they are invalid
       references in the context of MrDocs output.
    */
    void
    removeInvalidReferences(DocComment& doc);

    void
    removeInvalidReferences(Symbol& I);

    /*  Check the documentation for problems and creates warnings

        We first collect all warnings and then print
        them at once at the end of the finalization
        process. This way, the warnings can be sorted
        by location and the user can fix them in order.
     */
    void
    emitWarnings();

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
        Symbol const& ctx,
        report::Located<std::string_view> const format,
        Args&&... args)
    {
        MRDOCS_CHECK_OR(corpus_.config->warnings);
        auto loc = getPrimaryLocation(ctx);
        warn(*loc, format, std::forward<Args>(args)...);
    }

    void
    warnUndocumented();

    void
    warnDocErrors();

    void
    warnParamErrors(FunctionSymbol const& I);

    void
    warnNoParamDocs();

    void
    warnNoParamDocs(FunctionSymbol const& I);

    void
    warnUndocEnumValues();

    void
    warnUnnamedParams();

    void
    warnUnnamedParams(FunctionSymbol const& I);
};

} // mrdocs

#endif // MRDOCS_LIB_METADATA_FINALIZERS_DOCCOMMENTFINALIZER_HPP
