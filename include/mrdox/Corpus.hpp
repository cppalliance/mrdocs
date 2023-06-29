//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_CORPUS_HPP
#define MRDOX_API_CORPUS_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Config.hpp>
#include <mrdox/Metadata.hpp>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace clang {
namespace mrdox {

/** The collection of declarations in extracted form.
*/
class MRDOX_VISIBLE
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
    /** Destructor.
    */
    MRDOX_DECL
    virtual
    ~Corpus() noexcept;

    //--------------------------------------------

    /** The configuration used to generate this corpus.
    */
    Config const& config;

    /** Return a sorted index of all symbols.
    */
    MRDOX_DECL
    virtual
    std::vector<Info const*> const&
    index() const noexcept = 0;

    /** Return the metadata for the global namespace.
    */
    MRDOX_DECL
    NamespaceInfo const&
    globalNamespace() const noexcept;

    /** Return the Info with the matching ID, or nullptr.
    */
    MRDOX_DECL
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
        SpecializationInfo const& I,
        F&& f, Args&&... args) const;

    //--------------------------------------------

    // KRYSTIAN NOTE: temporary
    MRDOX_DECL
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
    MRDOX_ASSERT(I != nullptr);
    if constexpr(std::is_same_v<T, Info>)
    {
        return *I;
    }
    else
    {
        auto const& J = *static_cast<T const*>(I);
        MRDOX_ASSERT(J.Kind == T::kind_id);
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
    for(auto const& id : I.Specializations)
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
    for(auto const& id : I.Specializations)
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
    for(auto const& J : I.Members)
        visit(get(J.Specialized),
            std::forward<F>(f),
                std::forward<Args>(args)...);
}

} // mrdox
} // clang

#endif
