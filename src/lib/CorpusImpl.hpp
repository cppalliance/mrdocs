//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_CORPUSIMPL_HPP
#define MRDOCS_LIB_CORPUSIMPL_HPP

#include <mrdocs/Platform.hpp>
#include <lib/AST/ParseRef.hpp>
#include <lib/ConfigImpl.hpp>
#include <lib/Metadata/InfoSet.hpp>
#include <lib/Support/Debug.hpp>
#include <mrdocs/ADT/UnorderedStringMap.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>

namespace clang::mrdocs {

/** Implements the Corpus.

    The CorpusImpl class is the implementation of the Corpus interface.
    It is responsible for building the index of all symbols in the
    translation units, and providing access to the symbols via the
    iterator interface.

    The CorpusImpl class is not intended to be used directly. Instead,
    the Corpus interface can be used by plugins to access the symbols.
*/
class CorpusImpl final : public Corpus
{
    std::shared_ptr<ConfigImpl const> config_;

    // Info keyed on Symbol ID.
    InfoSet info_;

    // Undocumented symbols
    UndocumentedInfoSet undocumented_;

    // Lookup cache
    // The key represents the context symbol ID.
    // The value is another map from the name to the Info.
    std::map<SymbolID, UnorderedStringMap<Info const*>> lookupCache_;

    friend class Corpus;
    friend class BaseMembersFinalizer;
    friend class OverloadsFinalizer;
    friend class SortMembersFinalizer;
    friend class JavadocFinalizer;
    friend class NamespacesFinalizer;
    friend class DerivedFinalizer;

public:
    /** Constructor.
    */
    explicit
    CorpusImpl(
        std::shared_ptr<ConfigImpl const> config) noexcept
        : Corpus(*config)
        , config_(std::move(config))
    {
    }

    /** Iterator to the first Info.
    */
    iterator
    begin() const noexcept override;

    /** Iterator to one past the last Info.
    */
    iterator
    end() const noexcept override;

    /** Return the Info with the specified symbol ID.

        If the id does not exist, the behavior is undefined.
    */
    Info*
    find(SymbolID const& id) noexcept;

    Info const*
    find(SymbolID const& id) const noexcept override;

    /** Return a range of Info objects for the specified Symbol IDs.
     */
    template <range_of<SymbolID> R>
    auto
    find(R&& range)
    {
        return
            std::views::transform(
                range,
                [this](SymbolID const& id) -> Info*
                {
                    return this->find(id);
                }) |
            std::views::filter([](Info const* info)
                {
                    return info != nullptr;
                }) |
            std::views::transform([](Info* info) -> Info&
                {
                    return *info;
                }) |
            std::views::common;
    }

    /** Return a range of constant Info objects for the specified Symbol IDs.
     */
    template <range_of<SymbolID> R>
    auto
    find(R&& range) const
    {
        return
            std::views::transform(
                range,
                [this](SymbolID const& id) -> Info const*
                {
                    return this->find(id);
                }) |
            std::views::filter([](Info const* info)
                {
                    return info != nullptr;
                }) |
            std::views::transform([](Info const* info) -> Info const&
                {
                    return *info;
                }) |
            std::views::common;
    }

    Expected<std::reference_wrapper<Info const>>
    lookup(SymbolID const& context, std::string_view name) const override;

    Expected<std::reference_wrapper<Info const>>
    lookup(SymbolID const& context, std::string_view name);

    /** Build metadata for a set of translation units.

        This is the main point of interaction between MrDocs
        and the Clang Tooling infrastructure. The compilation
        database is used to build the index of all symbols
        in the translation units.

        Users of the MrDocs library via plugins will
        only have access to the Corpus interface whose
        instance will be already populated. They will
        not need to call this function directly.

        @param config A shared pointer to the configuration.
        @param compilations A compilations database for the input files.
    */
    // MRDOCS_DECL
    [[nodiscard]]
    static
    mrdocs::Expected<std::unique_ptr<Corpus>>
    build(
        std::shared_ptr<ConfigImpl const> const& config,
        tooling::CompilationDatabase const& compilations);

    void
    qualifiedName(
        Info const& I,
        std::string& result) const override;

    void
    qualifiedName(
        Info const& I,
        SymbolID const& context,
        std::string& result) const override;

    /** Finalize the corpus.
    */
    void
    finalize();

private:
    /** Return the Info with the specified symbol ID.

        If the id does not exist, the behavior is undefined.
    */
    template<class T>
    T&
    get(SymbolID const& id) noexcept;

    template <class Self>
    static
    Expected<std::reference_wrapper<Info const>>
    lookupImpl(
        Self&& self,
        SymbolID const& contextId,
        std::string_view name);

    template <class Self>
    static
    Info const*
    lookupImpl(
        Self&& self,
        SymbolID const& context,
        ParsedRef const& ref,
        std::string_view name,
        bool cache);

    Info const*
    lookupImpl(
        SymbolID const& contextId,
        ParsedRefComponent const& component,
        ParsedRef const& ref,
        bool checkParameters) const;

    std::pair<Info const*, bool>
    lookupCacheGet(
        SymbolID const& context,
        std::string_view name) const;

    void
    lookupCacheSet(
        SymbolID const& context,
        std::string_view name,
        Info const* info);

    void
    lookupCacheSet(
        SymbolID const&,
        std::string_view,
        Info const*) const
    {
        // no-op when const
    }
};

template<class T>
T&
CorpusImpl::
get(
    SymbolID const& id) noexcept
{
    auto I = find(id);
    MRDOCS_ASSERT(I != nullptr);
    auto const t = static_cast<T*>(I);
    if constexpr(! std::is_same_v<T, Info>)
        MRDOCS_ASSERT(t->Kind == T::kind_id);
    return *t;
}

} // clang::mrdocs

#endif // MRDOCS_LIB_CORPUSIMPL_HPP
