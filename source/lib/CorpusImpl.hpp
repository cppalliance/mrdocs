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

#ifndef MRDOX_SOURCE_CORPUSIMPL_HPP
#define MRDOX_SOURCE_CORPUSIMPL_HPP

#include "Bitcode.hpp"
#include <mrdox/Corpus.hpp>
#include <string>

namespace clang {
namespace mrdox {

/** Implements the Corpus.
*/
class CorpusImpl : public Corpus
{
public:
    /** Constructor.
    */
    explicit
    CorpusImpl(
        std::shared_ptr<Config const> config) noexcept
        : config_(std::move(config))
    {
    }

private:
    std::shared_ptr<Config const> const&
    config() const noexcept override
    {
        return config_;
    }

    std::vector<SymbolID> const&
    allSymbols() const noexcept override
    {
        return allSymbols_;
    }

    Info*
    find(
        SymbolID const& id) noexcept override;

    Info const*
    find(
        SymbolID const& id) const noexcept override;

    /** Return the Info with the specified symbol ID.

        If the id does not exist, the behavior is undefined.
    */
    template<class T>
    T&
    get(
        SymbolID const& id) noexcept;

    /** Insert this element and all its children into the Corpus.

        @param Thread Safety
        May be called concurrently.
    */
    void insert(std::unique_ptr<Info> Ip);

    /** Insert Info into the index

        @param Thread Safety
        May be called concurrently.
    */
    void insertIntoIndex(Info const& I);

private:
    struct Temps;
    friend class Corpus;

    /** Canonicalize the contents of the object.

        @return true upon success.

        @param R The diagnostic reporting object to
        use for delivering errors and information.
    */
    [[nodiscard]]
    bool canonicalize(Reporter& R);

    bool canonicalize(std::vector<SymbolID>& list, Temps& t, Reporter& R);
    bool canonicalize(NamespaceInfo& I, Temps& t, Reporter& R);
    bool canonicalize(RecordInfo& I, Temps& t, Reporter& R);
    bool canonicalize(FunctionInfo& I, Temps& t, Reporter& R);
    bool canonicalize(EnumInfo& I, Temps& t, Reporter& R);
    bool canonicalize(TypedefInfo& I, Temps& t, Reporter& R);
    bool canonicalize(Scope& I, Temps& t, Reporter& R);
    bool canonicalize(std::vector<Reference>& list, Temps& t, Reporter& R);
    bool canonicalize(llvm::SmallVectorImpl<MemberTypeInfo>& list, Temps& t, Reporter& R);

    std::shared_ptr<Config const> config_;

    // Index of all emitted symbols.
    Index Idx;

    // Table of Info keyed on Symbol ID.
    llvm::StringMap<std::unique_ptr<Info>> InfoMap;

    // list of all symbols
    std::vector<SymbolID> allSymbols_;

    llvm::sys::Mutex infoMutex;
    llvm::sys::Mutex allSymbolsMutex;
    bool isCanonical_ = false;
};

template<class T>
T&
CorpusImpl::
get(
    SymbolID const& id) noexcept
{
    auto I = find(id);
    assert(I != nullptr);
    auto const t = static_cast<T*>(I);
    if constexpr(std::is_same_v<T, NamespaceInfo>)
        assert(t->IT == InfoType::IT_namespace);
    else if constexpr(std::is_same_v<T, RecordInfo>)
        assert(t->IT == InfoType::IT_record);
    else if constexpr(std::is_same_v<T, FunctionInfo>)
        assert(t->IT == InfoType::IT_function);
    else if constexpr(std::is_same_v<T, EnumInfo>)
        assert(t->IT == InfoType::IT_enum);
    else if constexpr(std::is_same_v<T, TypedefInfo>)
        assert(t->IT == InfoType::IT_typedef);
    return *t;
}

} // mrdox
} // clang

#endif
