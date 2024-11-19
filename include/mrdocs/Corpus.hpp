//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_CORPUS_HPP
#define MRDOCS_API_CORPUS_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Metadata.hpp>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace clang {
namespace mrdocs {

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
    MRDOCS_DECL
    virtual
    iterator
    begin() const noexcept = 0;

    /** Return the end iterator for the index.
    */
    MRDOCS_DECL
    virtual
    iterator
    end() const noexcept = 0;

    /** Whether the corpus contains any symbols.
    */
    MRDOCS_DECL
    bool
    empty() const noexcept;

    /** Return the Info with the matching ID, or nullptr.
    */
    MRDOCS_DECL
    virtual
    Info const*
    find(SymbolID const& id) const noexcept = 0;

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
    MRDOCS_DECL
    NamespaceInfo const&
    globalNamespace() const noexcept;

    /** Visit the members of specified Info.

        This function invokes the specified function `f`
        for each member of the specified Info `I`.

        For each member of `I`, the function will invoke
        the function object `fn` with a type derived from
        `Info` as the first argument, followed by `args...`.

        The type of the first argument is determined
        by the `InfoKind` of the `Info` object.

        @param I The Info to visit.
        @param info The Info to visit.
        @param f The function to invoke.
        @param args The arguments to pass to the function.
    */
    template <InfoParent T, class F, class... Args>
    void
    traverse(
        T const& I, F&& f, Args&&... args) const
    {
        for (auto const& id : I.Members)
        {
            visit(get(id), std::forward<F>(f),
                  std::forward<Args>(args)...);
        }
    }

    /** Visit the member overloads of specified ScopeInfo.

        This function iterates the members of the
        specified ScopeInfo `S`.

        For each member in the scope, we check
        if the member is a function with overloads.

        If the member is a function with overloads,
        we create an @ref OverloadSet object and invoke
        the function object `f` with the @ref OverloadSet
        as the first argument, followed by `args...`.

        If the member is not a function with overloads,
        we invoke the function object `f` with the @ref Info
        member as the first argument, followed by `args...`.

    */
    template <class F, class... Args>
    void traverseOverloads(
        ScopeInfo const& S,
        F&& f,
        Args&&... args) const;

    //--------------------------------------------

    /** Return the fully qualified name of the specified Info.

        This function returns the fully qualified name
        of the specified Info `I` as a string.

        The `Info` parents are traversed to construct
        the fully qualified name which is stored in
        the string `temp`.

        @return A reference to the string `temp`.
     */
    // KRYSTIAN NOTE: temporary
    MRDOCS_DECL
    std::string&
    getFullyQualifiedName(
        const Info& I,
        std::string& temp) const;

    std::string
    getFullyQualifiedName(const Info& I) const
    {
        std::string temp;
        return getFullyQualifiedName(I, temp);
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

template <class F, class... Args>
void
Corpus::
traverseOverloads(
    ScopeInfo const& S,
    F&& f, Args&&... args) const
{
    MRDOCS_ASSERT(S.Members.empty() == S.Lookups.empty());
    for(const SymbolID& id : S.Members)
    {
        const Info& member = get(id);
        const auto& lookup = S.Lookups.at(member.Name);
        auto first_func = std::ranges::find_if(
            lookup, [this](const SymbolID& elem)
            {
            #if 0
                const Info& I = get(elem);
                return I.isFunction() || I.isGuide();
            #else
                return get(elem).isFunction();
            #endif
            });
        bool const nonOverloadedFunction = lookup.size() == 1;
        bool const notFunction = first_func == lookup.end();
        if(nonOverloadedFunction ||
           notFunction)
        {
            visit(member, std::forward<F>(f),
                std::forward<Args>(args)...);
        }
        else if(*first_func == id)
        {
            OverloadSet overloads(member.Name,
                member.Namespace.front(),
                member.Namespace, lookup);
            visit(overloads, std::forward<F>(f),
                std::forward<Args>(args)...);
        }
    }
}

class Corpus::iterator
{
    const Corpus* corpus_;
    const Info* val_;
    const Info*(*next_)(const Corpus*, const Info*);

public:
    using value_type = const Info;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    using const_pointer = const value_type*;
    using const_reference = const value_type&;

    iterator() = default;
    iterator(const iterator&) = default;
    iterator& operator=(const iterator&) = default;

    iterator(
        const Corpus* corpus,
        const Info* val,
        const Info*(*next)(const Corpus*, const Info*))
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

} // mrdocs
} // clang

#endif
