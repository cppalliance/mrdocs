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

#ifndef MRDOCS_LIB_METADATA_FINALIZER_JAVADOCFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZER_JAVADOCFINALIZER_HPP

#include "lib/Lib/CorpusImpl.hpp"
#include "lib/Lib/Info.hpp"
#include "mrdocs/Support/ScopeExit.hpp"
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
            return lhs.LineNumber > rhs.LineNumber;
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

    void
    build()
    {
        for (auto& I : corpus_.info_)
        {
            MRDOCS_ASSERT(I);
            MRDOCS_CHECK_OR_CONTINUE(I->Extraction != ExtractionMode::Dependency);
            visit(*I, *this);
        }
        emitWarnings();
    }

    void
    operator()(Info& I)
    {
        visit(I, *this);
    }

    // Finalize javadoc for this info object
    template <class InfoTy>
    void
    operator()(InfoTy& I);

private:
    // Look for symbol and set the id of a reference
    void
    finalize(doc::Reference& ref, bool emitWarning = true);

    // Recursively finalize references in a javadoc node
    void
    finalize(doc::Node& node);

    // Recursively finalize references in javadoc members
    void
    finalize(Javadoc& javadoc);

    // Copy brief and details to the current context
    void
    copyBriefAndDetails(Javadoc& javadoc);

    // Copy brief from first paragraph
    void
    setAutoBrief(Javadoc& javadoc);

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

    // Remove invalid ids from BaseInfo members
    void
    finalize(BaseInfo& info);

    // Remove invalid ids from TemplateInfo members
    void
    finalize(TemplateInfo& info);

    // Remove invalid ids from TypeInfo members
    void
    finalize(TypeInfo& type);

    // Remove invalid ids from NameInfo members
    void
    finalize(NameInfo& name);

    // Finalize optional and pointer-like members
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

    // Finalize a range of elements
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

    void
    emitWarnings();

    template<class... Args>
    void
    warn(
        Location const& loc,
        Located<std::string_view> const format,
        Args&&... args)
    {
        MRDOCS_CHECK_OR(corpus_.config->warnings);
        std::string const str = fmt::vformat(format.value, fmt::make_format_args(args...));
        warnings_[loc].push_back(str);
    }

    template<class... Args>
    void
    warn(
        Located<std::string_view> const format,
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

#endif
