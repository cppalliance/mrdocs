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

#include "ConfigImpl.hpp"
#include "Support/Debug.hpp"
#include <mrdox/Corpus.hpp>
#include <mrdox/Metadata.hpp>
#include <mrdox/Support/Expected.hpp>
#include <clang/Tooling/Execution.h>
#include <llvm/ADT/StringMap.h>
#include <llvm/Support/Mutex.h>
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


    /** Build metadata for a set of translation units.

        @param config A shared pointer to the configuration.
    */
    // MRDOX_DECL
    [[nodiscard]]
    static
    mrdox::Expected<std::unique_ptr<Corpus>>
    build(
        tooling::ToolExecutor& ex,
        std::shared_ptr<Config const> config);

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

private:
    struct Temps;
    friend class Corpus;

    std::shared_ptr<ConfigImpl const> config_;

    // Table of Info keyed on Symbol ID.
    llvm::StringMap<std::unique_ptr<Info>> InfoMap;
    std::vector<Info const*> index_;

    llvm::sys::Mutex mutex_;
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
    if constexpr(! std::is_same_v<T, Info>)
    {
        if constexpr(T::isNamespace())
            Assert(t->Kind == InfoKind::Namespace);
        else if constexpr(T::isRecord())
            Assert(t->Kind == InfoKind::Record);
        else if constexpr(T::isFunction())
            Assert(t->Kind == InfoKind::Function);
        else if constexpr(T::isTypedef())
            Assert(t->Kind == InfoKind::Typedef);
        else if constexpr(T::isEnum())
            Assert(t->Kind == InfoKind::Enum);
        else if constexpr(T::isVariable())
            Assert(t->Kind == InfoKind::Variable);
        else if constexpr(T::isField())
            Assert(t->Kind == InfoKind::Field);
        else if constexpr(T::isSpecialization())
            Assert(t->Kind == InfoKind::Specialization);
    }
    return *t;
}

} // mrdox
} // clang

#endif
