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


    class iterator;
public:

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

    /** Return the metadata for the global namespace.
    */
    MRDOCS_DECL
    NamespaceInfo const&
    globalNamespace() const noexcept;

    /** Return the Info with the matching ID, or nullptr.
    */
    MRDOCS_DECL
    virtual
    Info const*
    find(SymbolID const& id) const noexcept = 0;

    /** Return true if an Info with the specified symbol ID exists.
    */
    bool
    exists(
        SymbolID const& id) const noexcept
    {
        return find(id) != nullptr;
    }

    /** Return the Info with the specified symbol ID.

        If the id does not exist, the behavior is undefined.
    */
    template<class T = Info>
    T const&
    get(
        SymbolID const& id) const noexcept;

    //--------------------------------------------

    // KRYSTIAN FIXME: this could just be a single
    // overload constrained with std::derived_from<ScopeInfo>
    template<class F, class... Args>
    void traverse(
        NamespaceInfo const& I,
        F&& f, Args&&... args) const;

    template<class F, class... Args>
    void traverse(
        RecordInfo const& I,
        F&& f, Args&&... args) const;

    template<class F, class... Args>
    void traverse(
        EnumInfo const& I,
        F&& f, Args&&... args) const;

    template<class F, class... Args>
    void traverse(
        SpecializationInfo const& I,
        F&& f, Args&&... args) const;

    template<class F, class... Args>
    void traverse(
        OverloadSet const& OS,
        F&& f, Args&&... args) const;

    template<class F, class... Args>
    void traverseOverloads(
        ScopeInfo const& S,
        F&& f, Args&&... args) const;

    //--------------------------------------------

    // KRYSTIAN NOTE: temporary
    MRDOCS_DECL
    std::string&
    getFullyQualifiedName(
        const Info& I,
        std::string& temp) const;
};

//------------------------------------------------

template<class T>
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

template<class F, class... Args>
void
Corpus::
traverse(
    NamespaceInfo const& I,
    F&& f, Args&&... args) const
{
    for(auto const& id : I.Members)
        visit(get(id), std::forward<F>(f),
            std::forward<Args>(args)...);
}

template<class F, class... Args>
void
Corpus::
traverse(
    RecordInfo const& I,
    F&& f, Args&&... args) const
{
    for(auto const& id : I.Members)
        visit(get(id), std::forward<F>(f),
            std::forward<Args>(args)...);
}

template<class F, class... Args>
void
Corpus::
traverse(
    EnumInfo const& I,
    F&& f, Args&&... args) const
{
    for(auto const& id : I.Members)
        visit(get(id), std::forward<F>(f),
            std::forward<Args>(args)...);
}

template<class F, class... Args>
void
Corpus::
traverse(
    SpecializationInfo const& I,
    F&& f, Args&&... args) const
{
    for(auto const& id : I.Members)
        visit(get(id), std::forward<F>(f),
            std::forward<Args>(args)...);
}

template<class F, class... Args>
void
Corpus::
traverse(
    OverloadSet const& OS,
    F&& f, Args&&... args) const
{
    for(auto const& id : OS.Members)
        visit(get(id), std::forward<F>(f),
            std::forward<Args>(args)...);
}

template<class F, class... Args>
void
Corpus::
traverseOverloads(
    ScopeInfo const& S,
    F&& f, Args&&... args) const
{
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
        if(lookup.size() == 1 ||
            first_func == lookup.end())
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
