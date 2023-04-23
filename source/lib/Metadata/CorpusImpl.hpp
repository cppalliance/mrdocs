//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_CORPUSIMPL_HPP
#define MRDOX_LIB_CORPUSIMPL_HPP

#include <mrdox/Platform.hpp>
#include "AST/Bitcode.hpp"
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

    //--------------------------------------------

    /** Base class used to visit elements of the corpus.
    */
    struct MutableVisitor
    {
        virtual ~MutableVisitor() = default;
        virtual void visit(NamespaceInfo&) {}
        virtual void visit(RecordInfo&) {}
        virtual void visit(Overloads&) {}
        virtual void visit(FunctionInfo&) {}
        virtual void visit(TypedefInfo&) {}
        virtual void visit(EnumInfo&) {}
    };

    /** Visit the specified symbol ID or node.
    */
    /** @{ */
    void visit(SymbolID id, MutableVisitor& f);
    void visit(Scope& I, MutableVisitor& f);
    void visit(Info& I, MutableVisitor& f);
    /** @} */

private:
    struct Temps;
    class Canonicalizer;
    friend class Corpus;

    /** Canonicalize the contents of the object.

        @param R The diagnostic reporting object to
        use for delivering errors and information.
    */
    void canonicalize(Reporter& R);

    std::shared_ptr<Config const> config_;

    // Table of Info keyed on Symbol ID.
    llvm::StringMap<std::unique_ptr<Info>> InfoMap;
    std::vector<SymbolID> allSymbols_;

    llvm::sys::Mutex mutex_;
    bool isCanonical_ = false;
};

template<class T>
T&
CorpusImpl::
get(
    SymbolID const& id) noexcept
{
    auto I = find(id);
    Assert(I != nullptr);
    auto const t = static_cast<T*>(I);
    if constexpr(std::is_same_v<T, NamespaceInfo>)
        Assert(t->IT == InfoType::IT_namespace);
    else if constexpr(std::is_same_v<T, RecordInfo>)
        Assert(t->IT == InfoType::IT_record);
    else if constexpr(std::is_same_v<T, FunctionInfo>)
        Assert(t->IT == InfoType::IT_function);
    else if constexpr(std::is_same_v<T, EnumInfo>)
        Assert(t->IT == InfoType::IT_enum);
    else if constexpr(std::is_same_v<T, TypedefInfo>)
        Assert(t->IT == InfoType::IT_typedef);
    return *t;
}

} // mrdox
} // clang

#endif
