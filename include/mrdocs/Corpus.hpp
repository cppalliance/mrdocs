//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_CORPUS_HPP
#define MRDOCS_API_CORPUS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/ADT/UnorderedStringMap.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/TagfileIndex.hpp>
#include <mrdocs/detail/Corpus.hpp>
#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace mrdocs {

/// A parsed symbol reference (see lib/AST/ParseRef.hpp).
struct ParsedRef;
/// A component of a parsed symbol reference.
struct ParsedRefComponent;
/// The compilation database driving extraction.
class MrDocsCompilationDatabase;

/** The collection of declarations in extracted form.

    A corpus is a plain value holding the extracted symbols, so it can be
    moved freely and stays valid wherever it is built. It does not own the
    configuration: the @ref Config that drives a build is passed to @ref
    build, and again to whatever consumes the corpus (for example a
    @ref Generator). Use @ref build to produce one from a @ref Config.
*/
class MRDOCS_VISIBLE
    Corpus
{
public:
    /** Build a Corpus from a configuration.

        Drives extraction over the translation units described by the
        configuration. The compilation database is resolved from the
        configuration settings (a `compile_commands.json` path, a
        `CMakeLists.txt` plus `cmake` options, or a database synthesized
        from `source-root` and `input`).

        @param config The Config used for extraction.
        @return The populated Corpus on success, otherwise an Error.
    */
    MRDOCS_DECL
    static
    Expected<Corpus>
    build(Config const& config);

    /** Build a Corpus from a configuration and a compilation database.

        @param config The Config used for extraction.
        @param compilations The compilation database for the input files.
        @return The populated Corpus on success, otherwise an Error.
    */
    MRDOCS_DECL
    static
    Expected<Corpus>
    build(
        Config const& config,
        MrDocsCompilationDatabase const& compilations);

    /** The iterator type for the index of all symbols.

        A forward iterator over all symbols in the index, dereferencing to a
        reference to a const @ref Symbol.

        @seebelow
    */
    class iterator;

    //--------------------------------------------

    /** Return the begin iterator for the index of all symbols.
    */
    iterator
    begin() const noexcept;

    /** Return the end iterator for the index.
    */
    iterator
    end() const noexcept;

    /** Whether the corpus contains any symbols.

        The global namespace is a regular symbol in our model and is always
        present after a successful build, so a corpus is empty only in
        degenerate cases. To detect the common "nothing was extracted" case,
        compare @ref size against 1.

        @return true if the corpus is empty, otherwise false.
    */
    bool
    empty() const noexcept;

    /** Return the number of symbols in the corpus.

        The global namespace is always counted when present, so a size of 1
        means no declaration other than the global namespace was extracted.

        @return The number of symbols in the corpus.
    */
    std::size_t
    size() const noexcept
    {
        return info_.size();
    }

    /** Return the Symbol for the matching string in a given context.

        @param context The context to look up the symbol in.
        @param name The name of the symbol to look up.
        @return The matching Symbol, or an error if not found. If multiple
            symbols match, one is returned arbitrarily.
    */
    Expected<Symbol const&>
    lookup(SymbolID const& context, std::string_view name) const;

    /** Return the URL documenting a name this corpus does not hold.

        The counterpart of @ref lookup for the symbols covered by
        another documentation set: the name is resolved from the context
        outward, so one written relative to its enclosing scope reaches
        an external symbol just as it would reach one of ours. What
        comes back is a URL rather than a Symbol, since nothing was
        extracted to point at.

        A name no enclosing scope accounts for is not matched against
        a longer one: `vector` reaches `std::vector` only from inside
        `std`, exactly as it would for a symbol of this corpus.

        @param context The context the name is written in.
        @param name The name of the symbol to look up.
        @return The URL, or nothing if no tagfile documents the name.
    */
    std::optional<std::string>
    externalUrl(SymbolID const& context, std::string_view name) const;

    /** Return the Symbol with the matching ID, or nullptr.
    */
    Symbol const*
    find(SymbolID const& id) const noexcept;

    /** Return the Symbol for the matching string in the global context.

        @param name The name of the symbol to look up.
        @return The matching Symbol, or an error if not found.
    */
    Expected<Symbol const&>
    lookup(std::string_view name) const
    {
        return lookup(SymbolID::global, name);
    }

    /** Return true if a Symbol with the specified symbol ID exists.
    */
    bool
    exists(
        SymbolID const& id) const noexcept
    {
        return find(id) != nullptr;
    }

    /** Return the Symbol with the specified symbol ID.

        Uses @ref find and converts the result to `T`. The behavior is
        undefined if the id does not exist or the Symbol is not of type `T`.
    */
    template<class T = Symbol>
    requires std::derived_from<T, Symbol>
    T const&
    get(SymbolID const& id) const noexcept;

    /** Return the metadata for the global namespace.
    */
    NamespaceSymbol const&
    globalNamespace() const noexcept;

    /** Visit the specified Symbol IDs.

        Invokes `f` for each member of `range` with a type derived from
        `Symbol` as the first argument, followed by `args...`.

        @param range A range of SymbolID objects.
        @param f The function to invoke.
        @param args The arguments to pass to the function.
    */
    template <range_of<SymbolID> R, class F, class... Args>
    void
    visitIDs(R&& range, F&& f, Args&&... args) const
    {
        for (SymbolID const& id : range)
        {
            auto const* I = find(id);
            MRDOCS_CHECK_OR_CONTINUE(I);
            visit(*I, std::forward<F>(f), std::forward<Args>(args)...);
        }
    }

    /** Options to traverse the members of a Symbol.
    */
    struct TraverseOptions
    {
        /// Whether to traverse in a stable order
        bool ordered = false;
        /// Whether to skip inherited members whose parent is not the Symbol
        bool skipInherited = false;
        /// Whether to recurse into members
        bool recursive = false;
    };

    /** Visit the members of the specified Symbol.

        @param opts The options to traverse.
        @param I The Symbol to visit.
        @param f The function to invoke.
        @param args The arguments to pass to the function.
    */
    template <std::derived_from<Symbol> T, class F, class... Args>
    void
    traverse(TraverseOptions const& opts, T const& I, F&& f, Args&&... args) const
    {
        if constexpr (SymbolParent<T>)
        {
            if (!opts.ordered)
            {
                if (!opts.skipInherited)
                {
                    auto MS = allMembers(I);
                    visitIDs(MS,
                        std::forward<F>(f),
                        std::forward<Args>(args)...);
                    for (SymbolID const& id : MS)
                    {
                        auto const* MI = find(id);
                        MRDOCS_CHECK_OR_CONTINUE(MI);
                        traverse(opts, *MI, std::forward<F>(f), std::forward<Args>(args)...);
                    }
                }
                else /* skipInherited */
                {
                    auto nonInherited =
                        allMembers(I) |
                        std::views::filter([this, &I](SymbolID const& id) {
                            Symbol const* MI = find(id);
                            MRDOCS_CHECK_OR(MI, false);
                            return MI->Parent == I.id;
                        });
                    visitIDs(nonInherited,
                        std::forward<F>(f),
                        std::forward<Args>(args)...);
                    if (opts.recursive)
                    {
                        for (SymbolID const& id : nonInherited)
                        {
                            auto const* MI = find(id);
                            MRDOCS_CHECK_OR_CONTINUE(MI);
                            traverse(opts, *MI, std::forward<F>(f), std::forward<Args>(args)...);
                        }
                    }
                }
            }
            else /* ordered */
            {
                auto members0 = allMembers(I);
                static_assert(range_of<decltype(members0), SymbolID>);
                std::vector<SymbolID> members;
                members.reserve(std::ranges::distance(members0));
                std::ranges::copy(members0, std::back_inserter(members));
                std::ranges::sort(members,
                    [this](SymbolID const& lhs, SymbolID const& rhs)
                    {
                        auto const& lhsSymbol = get(lhs);
                        auto const& rhsSymbol = get(rhs);
                        if (auto const cmp = lhsSymbol.Name <=> rhsSymbol.Name;
                            !std::is_eq(cmp))
                        {
                            return std::is_lt(cmp);
                        }
                        return std::is_lt(CompareDerived(lhsSymbol, rhsSymbol));
                    });
                if (!opts.skipInherited)
                {
                    visitIDs(members,
                        std::forward<F>(f),
                        std::forward<Args>(args)...);
                    if (opts.recursive)
                    {
                        for (SymbolID const& id : members)
                        {
                            auto const* MI = find(id);
                            MRDOCS_CHECK_OR_CONTINUE(MI);
                            traverse(opts, *MI, std::forward<F>(f), std::forward<Args>(args)...);
                        }
                    }
                }
                else /* skipInherited */
                {
                    auto nonInherited =
                        members |
                        std::views::filter([this, &I](SymbolID const& id) {
                            Symbol const* MI = find(id);
                            MRDOCS_CHECK_OR(MI, false);
                            return MI->Parent == I.id;
                        });
                    visitIDs(nonInherited,
                        std::forward<F>(f),
                        std::forward<Args>(args)...);
                    if (opts.recursive)
                    {
                        for (SymbolID const& id : nonInherited)
                        {
                            auto const* MI = find(id);
                            MRDOCS_CHECK_OR_CONTINUE(MI);
                            traverse(opts, *MI, std::forward<F>(f), std::forward<Args>(args)...);
                        }
                    }
                }
            }
        }
    }

    /** Visit the members of the specified Symbol.

        @param I The Symbol to visit.
        @param f The function to invoke.
        @param args The arguments to pass to the function.
    */
    template <std::derived_from<Symbol> T, class F, class... Args>
    void
    traverse(T const& I, F&& f, Args&&... args) const
    {
        traverse({}, I, std::forward<F>(f), std::forward<Args>(args)...);
    }

    /** Populate `temp` with the fully qualified name of `I`.
        @param I The Symbol to get the qualified name for.
        @param temp The string to store the result in.
    */
    void
    qualifiedName(
        Symbol const& I,
        std::string& temp) const;

    /** Return the fully qualified name of `I`.
    */
    std::string
    qualifiedName(Symbol const& I) const
    {
        std::string temp;
        qualifiedName(I, temp);
        return temp;
    }

    /** Populate `result` with a qualified name relative to `context`.
        @param I The Symbol to name.
        @param context The context used to qualify the name.
        @param result Output string receiving the name.
    */
    void
    qualifiedName(
        Symbol const& I,
        SymbolID const& context,
        std::string& result) const;

    /** Return the qualified name of `I` relative to `context`.
    */
    std::string
    qualifiedName(Symbol const& I, SymbolID const& context) const
    {
        std::string temp;
        qualifiedName(I, context, temp);
        return temp;
    }

    /** Finalize the corpus.
    */
    void
    finalize(Config const& config);

private:
    Corpus() noexcept = default;

    // Info keyed on Symbol ID.
    detail::SymbolSet info_;

    // Undocumented symbols.
    detail::UndocumentedSymbolSet undocumented_;

    // Symbols documented elsewhere, read from the configured tagfiles.
    TagfileIndex externalSymbols_;

    // Lookup cache: context Symbol ID -> (name -> Info).
    std::map<SymbolID, UnorderedStringMap<Symbol const*>> lookupCache_;

    friend class BaseMembersFinalizer;
    friend class OverloadsFinalizer;
    friend class SortMembersFinalizer;
    friend class DocCommentFinalizer;
    friend class NamespacesFinalizer;
    friend class DerivedFinalizer;
    friend class FunctionObjectFinalizer;
    friend class SpecializationFinalizer;

public:

    /** Return the Symbol with the specified symbol ID, or nullptr.
    */
    Symbol*
    find(SymbolID const& id) noexcept;

    /** Return a range of Info objects for the specified Symbol IDs.

        @param range A range of Symbol IDs.
        @return A view of the matching Symbols.
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

    /** Return a range of const Info objects for the specified Symbol IDs.

        @param range A range of Symbol IDs.
        @return A view of the matching const Symbols.
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

    /// @copydoc lookup(SymbolID const&, std::string_view) const

    Expected<Symbol const&>
    lookup(SymbolID const& context, std::string_view name);

private:

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

//------------------------------------------------

template<class T>
requires std::derived_from<T, Symbol>
T const&
Corpus::
get(
    SymbolID const& id) const noexcept
{
    auto I = find(id);
    MRDOCS_ASSERT(I != nullptr);
    if constexpr(std::is_same_v<T, Symbol>)
    {
        return *I;
    }
    else
    {
        auto const& J = *static_cast<T const*>(I);
        MRDOCS_ASSERT(J.Kind == T::kind_id);
        return J;
    }
}

template<class T>
T&
Corpus::
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

class Corpus::iterator
{
    Corpus const* corpus_;
    Symbol const* val_;
    Symbol const*(*next_)(Corpus const*, Symbol const*);

public:
    /** Value type yielded by the iterator.
    */
    using value_type = const Symbol;
    /** Unsigned size type.
    */
    using size_type = std::size_t;
    /** Signed difference type.
    */
    using difference_type = std::ptrdiff_t;
    /** Pointer to value.
    */
    using pointer = value_type*;
    /** Reference to value.
    */
    using reference = value_type&;
    /** Pointer to const value.
    */
    using const_pointer = value_type const*;
    /** Reference to const value.
    */
    using const_reference = value_type const&;

    /** Default constructor.
    */
    iterator() = default;
    /** Copy constructor.
    */
    iterator(iterator const&) = default;
    /** Copy assignment.
    */
    iterator& operator=(iterator const&) = default;

    /** Construct an iterator from corpus storage pointers.
        @param corpus The parent corpus.
        @param val The current symbol.
        @param next Function that advances to the next symbol.
    */
    iterator(
        Corpus const* corpus,
        Symbol const* val,
        Symbol const*(*next)(Corpus const*, Symbol const*))
        : corpus_(corpus)
        , val_(val)
        , next_(next)
    {
    }

    /** Pre-increment.
        @return *this advanced to next element.
    */
    iterator& operator++() noexcept
    {
        MRDOCS_ASSERT(val_);
        val_ = next_(corpus_, val_);
        return *this;
    }

    /** Post-increment.
        @param dummy Unused postfix increment discriminator.
        @return Iterator prior to increment.
    */
    iterator operator++(int dummy) noexcept
    {
        (void)dummy;
        MRDOCS_ASSERT(val_);
        auto temp = *this;
        val_ = next_(corpus_, val_);
        return temp;
    }

    /** Pointer-like access to the current symbol.
        @return Pointer to the current Symbol.
    */
    const_pointer operator->() const noexcept
    {
        MRDOCS_ASSERT(val_);
        return val_;
    }

    /** Dereference to the current symbol.
        @return Reference to the current Symbol.
    */
    const_reference operator*() const noexcept
    {
        MRDOCS_ASSERT(val_);
        return *val_;
    }

    /** Equality comparison.
    */
    bool operator==(iterator const& other) const noexcept
    {
        return val_ == other.val_;
    }

    /** Inequality comparison.
    */
    bool operator!=(iterator const& other) const noexcept
    {
        return val_ != other.val_;
    }
};

/** Return the parents of the specified symbol

    The parents are returned in order from the root to the immediate parent.

    @param C The corpus containing the symbol.
    @param I The symbol to get the parents of.
    @return A vector of SymbolIDs representing the parents of `I`.
*/
MRDOCS_DECL
std::vector<SymbolID>
getParents(Corpus const& C, Symbol const& I);

} // mrdocs

#endif // MRDOCS_API_CORPUS_HPP
