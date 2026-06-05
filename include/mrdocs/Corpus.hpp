//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_CORPUS_HPP
#define MRDOCS_API_CORPUS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Metadata.hpp>
#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace mrdocs {

/** The collection of declarations in extracted form.
*/
class MRDOCS_VISIBLE
    Corpus
{
protected:
    /** Create a corpus using the provided configuration.
    */
    explicit
    Corpus(
        Config const& config_) noexcept
        : config(config_)
    {
    }
public:
    /** Build a Corpus from a configuration.

        Drives extraction over the translation units described by the
        configuration. The compilation database is resolved from the
        configuration settings (a `compile_commands.json` path, a
        `CMakeLists.txt` plus `cmake` options, or a database
        synthesized from `source-root` and `input`).

        @param config The Config used for extraction, as returned
                      by @ref Config::load.
        @return A unique pointer to the populated Corpus on success,
                otherwise an Error.
    */
    MRDOCS_DECL
    static
    Expected<std::unique_ptr<Corpus>>
    build(std::shared_ptr<Config const> const& config);

    /** The iterator type for the index of all symbols.

        The iterator is a forward iterator that
        iterates over all symbols in the index.
        It dereferences to a reference to a
        const @ref Symbol.

        The logic for incrementing the iterator is
        provided by the Corpus implementation via
        a function that retuns the next Symbol in the
        index, or nullptr if there are no more.
    */
    class iterator;

    /** Destructor.
    */
    MRDOCS_DECL
    virtual
    ~Corpus() noexcept;

    //--------------------------------------------

    /** The configuration used to generate this corpus.
    */
    Config const& config;

    /** Return the begin iterator for the index of all symbols.
    */
    virtual
    iterator
    begin() const noexcept = 0;

    /** Return the end iterator for the index.
    */
    virtual
    iterator
    end() const noexcept = 0;

    /** Whether the corpus contains any symbols.

        @return true if the corpus is empty, otherwise false.
    */
    bool
    empty() const noexcept;

    /** Return the Symbol for the matching string in a given context.

        @param context The context to look up the symbol in.
        @param name The name of the symbol to look up.
        @return The Symbol for the symbol with the specified name
            in the specified context, or an error if not found.
            If multiple symbols match, one is returned arbitrarily.
            Use @ref traverse to find all matching symbols.
    */
    virtual
    Expected<Symbol const&>
    lookup(SymbolID const& context, std::string_view name) const = 0;

    /** Return the Symbol with the matching ID, or nullptr.
    */
    virtual
    Symbol const*
    find(SymbolID const& id) const noexcept = 0;

    /** Return the Symbol for the matching string in the global context.

        @param name The name of the symbol to look up.
        @return The Symbol for the symbol with the specified name
            in the global context, or an error if not found.
    */
    Expected<Symbol const&>
    lookup(std::string_view name) const
    {
        return lookup(SymbolID::global, name);
    }

    /** Return true if an Symbol with the specified symbol ID exists.

        This function uses the @ref find function to locate
        the Symbol with the specified symbol ID and returns
        true if it exists, otherwise false.
    */
    bool
    exists(
        SymbolID const& id) const noexcept
    {
        return find(id) != nullptr;
    }

    /** Return the Symbol with the specified symbol ID.

        This function uses the @ref find function to locate
        the Symbol with the specified symbol ID. The result
        is converted to the specified type T and returned.

        The function @ref exists can be used to determine
        if an Symbol with the specified symbol ID exists.
        If the id does not exist, the behavior is undefined.

        If the Symbol is not of type T, the behavior is undefined.
    */
    template<class T = Symbol>
    requires std::derived_from<T, Symbol>
    T const&
    get(SymbolID const& id) const noexcept;

    /** Return the metadata for the global namespace.

        This function is equivalent to calling
        @ref get with the symbol ID for the
        global namespace.
    */
    NamespaceSymbol const&
    globalNamespace() const noexcept;

    /** Visit the specified Symbol IDs

        This function invokes the specified function `f`
        for each member of the specified range of Symbol IDs.

        For each member of `I` associated with the ID in `range`,
        the function will invoke the function object `fn` with a
        type derived from `Symbol` as the first argument, followed by
        `args...`.

        The type of the first argument is determined
        by the `SymbolKind` of the `Symbol` object.

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

    /** Options to traverse the members of an Symbol.
    */
    struct TraverseOptions
    {
        /// Whether to traverse in a stable order
        bool ordered = false;
        /// Whether to skip inherited members whose parent is not the Symbol
        bool skipInherited = false;
        /// Whether to skip inherited members whose parent is not the Symbol
        bool recursive = false;
    };

    /** Visit the members of specified Symbol.

        This function invokes the specified function `f`
        for each member of the specified Symbol `I`.

        For each member of `I`, the function will invoke
        the function object `fn` with a type derived from
        `Symbol` as the first argument, followed by `args...`.

        The type of the first argument is determined
        by the `SymbolKind` of the `Symbol` object.

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

    /** Visit the members of specified Symbol.

        This function invokes the specified function `f`
        for each member of the specified Symbol `I`.

        For each member of `I`, the function will invoke
        the function object `fn` with a type derived from
        `Symbol` as the first argument, followed by `args...`.

        The type of the first argument is determined
        by the `SymbolKind` of the `Symbol` object.

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
    virtual
    void
    qualifiedName(
        Symbol const& I,
        std::string& temp) const = 0;

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
        If `context` contains `I`, the name is relative; otherwise it
        is computed from the global namespace.
        @param I The Symbol to name.
        @param context The context used to qualify the name.
        @param result Output string receiving the name.
    */
    virtual
    void
    qualifiedName(
        Symbol const& I,
        SymbolID const& context,
        std::string& result) const = 0;

    /** Return the qualified name of `I` relative to `context`.
    */
    std::string
    qualifiedName(Symbol const& I, SymbolID const& context) const
    {
        std::string temp;
        qualifiedName(I, context, temp);
        return temp;
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

/** Return a list of the parent symbols of the specified Symbol.
*/
MRDOCS_DECL
std::vector<SymbolID>
getParents(Corpus const& C, Symbol const& I);

} // mrdocs

#endif // MRDOCS_API_CORPUS_HPP
