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
    std::set<std::pair<std::string, std::string>> warned_;
    std::set<Info const*> finalized_;

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
    finalize(doc::Reference& ref);

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
};

} // clang::mrdocs

#endif
