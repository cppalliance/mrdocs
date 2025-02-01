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

#ifndef MRDOCS_LIB_METADATA_FINALIZER_REFERENCEFINALIZER_HPP
#define MRDOCS_LIB_METADATA_FINALIZER_REFERENCEFINALIZER_HPP

#include "lib/Lib/Info.hpp"
#include "lib/Lib/Lookup.hpp"
#include <set>
#include <utility>

namespace clang::mrdocs {

/** Finalizes a set of Info.

    This removes any references to SymbolIDs
    which do not exist.

    References which should always be valid
    are not checked.
*/
class ReferenceFinalizer
{
    InfoSet& info_;
    SymbolLookup& lookup_;
    Info* current_ = nullptr;
    std::set<std::pair<std::string, std::string>> warned_;

    bool
    resolveReference(doc::Reference& ref) const;

    void
    finalize(SymbolID& id);

    void
    finalize(std::vector<SymbolID>& ids);

    void
    finalize(TArg& arg);

    void
    finalize(TParam& param);

    void
    finalize(Param& param);

    void
    finalize(BaseInfo& info);

    void
    finalize(TemplateInfo& info);

    void
    finalize(TypeInfo& type);

    void
    finalize(NameInfo& name);

    void
    finalize(doc::Node& node);

    void
    finalize(Javadoc& javadoc);

    template<typename T>
    void
    finalize(Optional<T>& val) requires
    requires { this->finalize(*val); }
    {
        if (val)
        {
            finalize(*val);
        }
    }

    template<typename T>
    void
    finalize(T* ptr) requires
    requires { this->finalize(*ptr); }
    {
        if (ptr)
        {
            finalize(*ptr);
        }
    }

    template<typename T>
    void
    finalize(std::unique_ptr<T>& ptr) requires
    requires { this->finalize(*ptr); }
    {
        if (ptr)
        {
            finalize(*ptr);
        }
    }

    template<typename T>
    void finalize(std::optional<T>& ptr) requires
    requires { this->finalize(*ptr); }
    {
        if (ptr)
        {
            finalize(*ptr);
        }
    }

    template<typename T>
    void finalize(Polymorphic<T>& v) requires
    requires { this->finalize(*v); }
    {
        if (v)
        {
            finalize(*v);
        }
    }

    template<typename Range>
    requires std::ranges::input_range<Range>
    void finalize(Range&& range)
    {
        for (auto&& elem: range)
        {
            finalize(elem);
        }
    }

    // ----------------------------------------------------------------

    // Check if SymbolID exists in info_
    void
    check(SymbolID const& id) const;

    template <range_of<SymbolID> T>
    void
    check(T&& ids) const
    {
        MRDOCS_ASSERT(std::ranges::all_of(ids,
            [this](const SymbolID& id)
            {
                return info_.contains(id);
            }));
    }


public:
    ReferenceFinalizer(
        InfoSet& Info,
        SymbolLookup& Lookup)
        : info_(Info)
        , lookup_(Lookup)
    {
    }

    void finalize(Info& I);

    void
    operator()(NamespaceInfo& I);

    void
    operator()(RecordInfo& I);

    void
    operator()(SpecializationInfo& I);

    void
    operator()(FunctionInfo& I);

    void
    operator()(TypedefInfo& I);

    void
    operator()(EnumInfo& I);

    void
    operator()(FieldInfo& I);

    void
    operator()(VariableInfo& I);

    void
    operator()(FriendInfo& I);

    void
    operator()(NamespaceAliasInfo& I);

    void
    operator()(UsingInfo& I);

    void
    operator()(EnumConstantInfo& I);

    void
    operator()(GuideInfo& I);

    void
    operator()(ConceptInfo& I);

    void
    operator()(OverloadsInfo& I);
};

} // clang::mrdocs

#endif
