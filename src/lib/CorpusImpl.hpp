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
#include <lib/Metadata/SymbolSet.hpp>
#include <lib/MrDocsCompilationDatabase.hpp>
#include <lib/Support/Debug.hpp>
#include <mrdocs/ADT/UnorderedStringMap.hpp>
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Error.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mrdocs {

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
    SymbolSet info_;

    // Undocumented symbols
    UndocumentedSymbolSet undocumented_;

    // Lookup cache
    // The key represents the context symbol ID.
    // The value is another map from the name to the Info.
    std::map<SymbolID, UnorderedStringMap<Symbol const*>> lookupCache_;

    // Output generators an extension script defined via
    // `register_generator(id, fn)`. Each `fn` is a `dom::Function` that
    // stays runnable until this corpus is destroyed (after extensions run,
    // when a generator is selected). Its scripting VM is kept alive either
    // by the function itself or by a matching entry in
    // `scriptVmKeepAlives_`. First registration of an id wins; later ones
    // are ignored.
    std::vector<std::pair<std::string, dom::Function>> scriptGenerators_;

    // Strong references to scripting VMs that back `scriptGenerators_` but
    // are only weakly held by the generator functions themselves (the
    // JavaScript backend works this way). Keeping a reference here lets
    // such a generator outlive the extension run that defined it, up to
    // this corpus's destruction.
    std::vector<std::shared_ptr<void>> scriptVmKeepAlives_;

    friend class Corpus;
    friend class BaseMembersFinalizer;
    friend class OverloadsFinalizer;
    friend class SortMembersFinalizer;
    friend class DocCommentFinalizer;
    friend class NamespacesFinalizer;
    friend class DerivedFinalizer;
    friend class FunctionObjectFinalizer;
    friend class SpecializationFinalizer;

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

    /** Return the number of symbols in the corpus.
    */
    std::size_t
    size() const noexcept override
    {
        return info_.size();
    }

    /** Return the Info with the specified symbol ID.

        If the id does not exist, the behavior is undefined.
    */
    Symbol*
    find(SymbolID const& id) noexcept;

    Symbol const*
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
                [this](SymbolID const& id) -> Symbol*
                {
                    return this->find(id);
                }) |
            std::views::filter([](Symbol const* info)
                {
                    return info != nullptr;
                }) |
            std::views::transform([](Symbol* info) -> Symbol&
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
                [this](SymbolID const& id) -> Symbol const*
                {
                    return this->find(id);
                }) |
            std::views::filter([](Symbol const* info)
                {
                    return info != nullptr;
                }) |
            std::views::transform([](Symbol const* info) -> Symbol const&
                {
                    return *info;
                }) |
            std::views::common;
    }

    Expected<Symbol const&>
    lookup(SymbolID const& context, std::string_view name) const override;

    Expected<Symbol const&>
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
        @param compilations A MrDocs compilations database for the input files.
    */
    // MRDOCS_DECL
    [[nodiscard]]
    static mrdocs::Expected<std::unique_ptr<Corpus>>
    build(
        std::shared_ptr<ConfigImpl const> const& config,
        MrDocsCompilationDatabase const& compilations);

    void
    qualifiedName(Symbol const& I,
        std::string& result) const override;

    void
    qualifiedName(Symbol const& I,
        SymbolID const& context,
        std::string& result) const override;

    /** Finalize the corpus.
    */
    void
    finalize();

    /** Register a script-defined output generator.

        Called from an extension's `register_generator(id, fn)`. The first
        registration of a given id wins; later ones are ignored.
    */
    void
    registerScriptGenerator(std::string id, dom::Function fn);

    /** Return the script-defined generator with this id, or `nullptr`.
    */
    dom::Function const*
    findScriptGenerator(std::string_view id) const noexcept;

    /** Keep a scripting VM alive for the lifetime of this corpus.

        A generator registered via `register_generator` may hold only a
        weak reference to the VM that defined it. The extension binding
        hands the VM over here so it outlives the extension run and stays
        usable when the generator is selected.
    */
    void
    keepScriptVmAlive(std::shared_ptr<void> keepAlive);

private:
    /** Return the Info with the specified symbol ID.

        If the id does not exist, the behavior is undefined.
    */
    template<class T>
    T&
    get(SymbolID const& id) noexcept;

    template <class Self>
    static
    Expected<Symbol const&>
    lookupImpl(
        Self&& self,
        SymbolID const& contextId,
        std::string_view name);

    template <class Self>
    static Symbol const*
    lookupImpl(
        Self&& self,
        SymbolID const& context,
        ParsedRef const& ref,
        std::string_view name,
        bool cache);

    Symbol const*
    lookupImpl(
        SymbolID const& contextId,
        ParsedRefComponent const& component,
        ParsedRef const& ref,
        bool checkParameters) const;

    std::pair<Symbol const*, bool>
    lookupCacheGet(
        SymbolID const& context,
        std::string_view name) const;

    void
    lookupCacheSet(
        SymbolID const& context,
        std::string_view name,
        Symbol const* info);

    void
    lookupCacheSet(
        SymbolID const&,
        std::string_view, Symbol const*) const
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
    if constexpr(! std::is_same_v<T, Symbol>)
        MRDOCS_ASSERT(t->Kind == T::kind_id);
    return *t;
}

} // mrdocs

#endif // MRDOCS_LIB_CORPUSIMPL_HPP
