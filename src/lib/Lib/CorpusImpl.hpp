//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_CORPUSIMPL_HPP
#define MRDOCS_LIB_CORPUSIMPL_HPP

#include "lib/Lib/ConfigImpl.hpp"
#include "lib/Lib/Info.hpp"
#include "lib/Support/Debug.hpp"
#include <mrdocs/Corpus.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Error.hpp>
#include <clang/Tooling/CompilationDatabase.h>
#include <mutex>
#include <string>

namespace clang {
namespace mrdocs {

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

    iterator begin() const noexcept override;
    iterator end() const noexcept override;

    Info*
    find(
        SymbolID const& id) noexcept;

    /** Build metadata for a set of translation units.

        @param reportLevel Error reporting level.
        @param config A shared pointer to the configuration.
        @param compilations A compilations database for the input files.
    */
    // MRDOCS_DECL
    [[nodiscard]]
    static
    mrdocs::Expected<std::unique_ptr<Corpus>>
    build(
        report::Level reportLevel,
        std::shared_ptr<ConfigImpl const> config,
        tooling::CompilationDatabase const& compilations);

private:
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

private:
    friend class Corpus;

    std::shared_ptr<ConfigImpl const> config_;

    // Info keyed on Symbol ID.
    InfoSet info_;
};

template<class T>
T&
CorpusImpl::
get(
    SymbolID const& id) noexcept
{
    auto I = find(id);
    MRDOCS_ASSERT(I != nullptr);
    auto const t = static_cast<T*>(I);
    if constexpr(! std::is_same_v<T, Info>)
        MRDOCS_ASSERT(t->Kind == T::kind_id);
    return *t;
}

} // mrdocs
} // clang

#endif
