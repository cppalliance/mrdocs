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

#ifndef MRDOX_CORPUS_HPP
#define MRDOX_CORPUS_HPP

#include "jad/Index.hpp"
#include <mrdox/Config.hpp>
#include <mrdox/Errors.hpp>
#include <clang/Tooling/Execution.h>

namespace clang {
namespace mrdox {

/** The collection of declarations in extracted form.
*/
struct Corpus
{
    Corpus() = default;
    Corpus(Corpus&&) noexcept = default;
    Corpus& operator=(Corpus const&) = delete;

    /** Index of all emitted symbols.
    */
    Index Idx;

    /** Table of Info keyed on USR.

        A USRs is a string that provide an
        unambiguous reference to a symbol.
    */
    llvm::StringMap<std::unique_ptr<Info>> USRToInfo;

    /** List of everything
    */
    std::vector<SymbolID> allSymbols;

    //--------------------------------------------

    /** Return a pointer to the Info with the matching USR, or nullptr.
    */
    Info const*
    find(
        SymbolID const& id) const noexcept;

    /** Insert Info into the index
    */
    void
    insert(
        Info const* I);
};

/** Build the intermediate representation of the code being documented.
*/
std::unique_ptr<Corpus>
buildCorpus(
    tooling::ToolExecutor& ex,
    Config const& cfg,
    Reporter& R);

} // mrdox
} // clang

#endif
