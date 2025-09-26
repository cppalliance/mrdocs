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
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace clang::mrdocs {

/** The collection of declarations in extracted form.
*/
class MRDOCS_VISIBLE
    Corpus
{
protected:
    explicit
    Corpus(
        Config const& config_) noexcept
        : config(config_)
    {
    }
public:
    /** The iterator type for the index of all symbols.

        The iterator is a forward iterator that
        iterates over all symbols in the index.
        It dereferences to a reference to a
        const @ref Info.

        The logic for incrementing the iterator is
        provided by the Corpus implementation via
        a function that retuns the next Info in the
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

    /** Return the Info with the matching ID, or nullptr.
    */
    virtual
    Info const*
    find(SymbolID const& id) const noexcept = 0;

    /** Return the Info for the matching string in a given context.

        @param context The context to look up the symbol in.
        @param name The name of the symbol to look up.
        @return The Info for the symbol with the specified name
            in the specified context, or an error if not found.
            If multiple symbols match, one is returned arbitrarily.
            Use @ref traverse to find all matching symbols.
    */
    virtual
    Expected<Info const&>
    lookup(SymbolID const& context, std::string_view name) const = 0;

    /** Return the Info for the matching string in the global context.

        @param name The name of the symbol to look up.
        @return The Info for the symbol with the specified name
            in the global context, or an error if not found.
    */
    Expected<Info const&>
    lookup(std::string_view name) const
    {
        return lookup(SymbolID::global, name);
    }

    /** Return true if an Info with the specified symbol ID exists.

        This function uses the @ref find function to locate
        the Info with the specified symbol ID and returns
        true if it exists, otherwise false.
    */
    bool
    exists(
        SymbolID const& id) const noexcept
    {
        return find(id) != nullptr;
    }

    /** Return the Info with the specified symbol ID.

        This function uses the @ref find function to locate
        the Info with the specified symbol ID. The result
        is converted to the specified type T and returned.

        The function @ref exists can be used to determine
        if an Info with the specified symbol ID exists.
        If the id does not exist, the behavior is undefined.

        If the Info is not of type T, the behavior is undefined.
    */
    template<class T = Info>
    requires std::derived_from<T, Info>
    T const&
    get(SymbolID const& id) const noexcept;

    /** Return the metadata for the global namespace.

        This function is equivalent to calling
        @ref get with the symbol ID for the
        global namespace.
    */
    NamespaceInfo const&
    globalNamespace() const noexcept;

    /** Visit the specified Symbol IDs

        This function invokes the specified function `f`
        for each member of the specified range of Symbol IDs.

        For each member of `I` associated with the ID in `range`,
        the function will invoke the function object `fn` with a
        type derived from `Info` as the first argument, followed by
        `args...`.

        The type of the first argument is determined
        by the `InfoKind` of the `Info` object.

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

    /** Options to traverse the members of an Info.
     */
    struct TraverseOptions
    {
        /// Whether to traverse in a stable order
        bool ordered = false;
        /// Whether to skip inherited members whose parent is not the Info
        bool skipInherited = false;
        /// Whether to skip inherited members whose parent is not the Info
        bool recursive = false;
    };

    /** Visit the members of specified Info.

        This function invokes the specified function `f`
        for each member of the specified Info `I`.

        For each member of `I`, the function will invoke
        the function object `fn` with a type derived from
        `Info` as the first argument, followed by `args...`.

        The type of the first argument is determined
        by the `InfoKind` of the `Info` object.

        @param opts The options to traverse.
        @param I The Info to visit.
        @param f The function to invoke.
        @param args The arguments to pass to the function.
    */
    template <std::derived_from<Info> T, class F, class... Args>
    void
    traverse(TraverseOptions const& opts, T const& I, F&& f, Args&&... args) const
    {
        if constexpr (InfoParent<T>)
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
                            Info const* MI = find(id);
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
                        auto const& lhsInfo = get(lhs);
                        auto const& rhsInfo = get(rhs);
                        if (auto const cmp = lhsInfo.Name <=> rhsInfo.Name;
                            !std::is_eq(cmp))
                        {
                            return std::is_lt(cmp);
                        }
                        return std::is_lt(CompareDerived(lhsInfo, rhsInfo));
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
                            Info const* MI = find(id);
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

    /** Visit the members of specified Info.

        This function invokes the specified function `f`
        for each member of the specified Info `I`.

        For each member of `I`, the function will invoke
        the function object `fn` with a type derived from
        `Info` as the first argument, followed by `args...`.

        The type of the first argument is determined
        by the `InfoKind` of the `Info` object.

        @param I The Info to visit.
        @param f The function to invoke.
        @param args The arguments to pass to the function.
    */
    template <std::derived_from<Info> T, class F, class... Args>
    void
    traverse(T const& I, F&& f, Args&&... args) const
    {
        traverse({}, I, std::forward<F>(f), std::forward<Args>(args)...);
    }

    /** Return the fully qualified name of the specified Info.

        This function returns the fully qualified name
        of the specified Info `I` as a string.

        The `Info` parents are traversed to construct
        the fully qualified name which is stored in
        the string `temp`.

        @param I The Info to get the qualified name for.
        @param temp The string to store the result in.
        @return A reference to the string `temp`.
     */
    virtual
    void
    qualifiedName(
        Info const& I,
        std::string& temp) const = 0;

    std::string
    qualifiedName(Info const& I) const
    {
        std::string temp;
        qualifiedName(I, temp);
        return temp;
    }

    /** Return a qualified name from the specified context.

        This function returns the qualified name
        of the specified Info `I` from the context
        specified by the SymbolID `context`.

        If the context is a parent of `I`, the
        qualified name is constructed relative to
        the context. For instance, if `I` is `A::B::C::D`
        and context is `A::B`, the result is `C::D`.

        If the context is not a parent of `I`, the
        qualified name is constructed relative to
        the global namespace with the prefix `::`.

        @param I The Info to get the qualified name for.
        @param context The context to get the qualified name from.
        @param result The string to store the result in.
     */
    virtual
    void
    qualifiedName(
        Info const& I,
        SymbolID const& context,
        std::string& result) const = 0;

    std::string
    qualifiedName(Info const& I, SymbolID const& context) const
    {
        std::string temp;
        qualifiedName(I, context, temp);
        return temp;
    }
};

//------------------------------------------------

template<class T>
requires std::derived_from<T, Info>
T const&
Corpus::
get(
    SymbolID const& id) const noexcept
{
    auto I = find(id);
    MRDOCS_ASSERT(I != nullptr);
    if constexpr(std::is_same_v<T, Info>)
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
    Info const* val_;
    Info const*(*next_)(Corpus const*, Info const*);

public:
    using value_type = const Info;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    using const_pointer = value_type const*;
    using const_reference = value_type const&;

    iterator() = default;
    iterator(iterator const&) = default;
    iterator& operator=(iterator const&) = default;

    iterator(
        Corpus const* corpus,
        Info const* val,
        Info const*(*next)(Corpus const*, Info const*))
        : corpus_(corpus)
        , val_(val)
        , next_(next)
    {
    }

    iterator& operator++() noexcept
    {
        MRDOCS_ASSERT(val_);
        val_ = next_(corpus_, val_);
        return *this;
    }

    iterator operator++(int) noexcept
    {
        MRDOCS_ASSERT(val_);
        auto temp = *this;
        val_ = next_(corpus_, val_);
        return temp;
    }

    const_pointer operator->() const noexcept
    {
        MRDOCS_ASSERT(val_);
        return val_;
    }

    const_reference operator*() const noexcept
    {
        MRDOCS_ASSERT(val_);
        return *val_;
    }

    bool operator==(iterator const& other) const noexcept
    {
        return val_ == other.val_;
    }

    bool operator!=(iterator const& other) const noexcept
    {
        return val_ != other.val_;
    }
};

/** Return a list of the parent symbols of the specified Info.
 */
MRDOCS_DECL
std::vector<SymbolID>
getParents(Corpus const& C, Info const& I);

} // clang::mrdocs

#endif // MRDOCS_API_CORPUS_HPP
