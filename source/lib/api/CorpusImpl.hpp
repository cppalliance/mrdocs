//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_CORPUSIMPL_HPP
#define MRDOX_API_CORPUSIMPL_HPP

#include "api/ConfigImpl.hpp"
#include "api/Support/Debug.hpp"
#include <mrdox/Corpus.hpp>
#include <llvm/ADT/StringMap.h>
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
        std::shared_ptr<ConfigImpl const> config) noexcept
        : Corpus(*config)
        , config_(std::move(config))
    {
    }

    Info*
    find(
        SymbolID const& id) noexcept;

private:
    std::vector<Info const*> const&
    index() const noexcept override
    {
        return index_;
    }

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
        virtual void visit(FunctionInfo&) {}
        virtual void visit(TypedefInfo&) {}
        virtual void visit(EnumInfo&) {}
        virtual void visit(VarInfo&) {}
    };

    /** Visit the specified symbol ID or node.
    */
    /** @{ */
    void traverse(MutableVisitor& f, Info& I);
    void traverse(MutableVisitor& f, NamespaceInfo& I);
    void traverse(MutableVisitor& f, RecordInfo& I);
    void traverse(MutableVisitor& f, SymbolID id);
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

    std::shared_ptr<ConfigImpl const> config_;

    // Table of Info keyed on Symbol ID.
    llvm::StringMap<std::unique_ptr<Info>> InfoMap;
    std::vector<Info const*> index_;

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
    else if constexpr(std::is_same_v<T, TypedefInfo>)
        Assert(t->IT == InfoType::IT_typedef);
    else if constexpr(std::is_same_v<T, EnumInfo>)
        Assert(t->IT == InfoType::IT_enum);
    return *t;
}

} // mrdox
} // clang

#endif
