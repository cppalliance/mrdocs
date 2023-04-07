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
#include <mutex>

namespace clang {
namespace mrdox {

/** The collection of declarations in extracted form.
*/
struct Corpus
{
    Corpus();
    Corpus(Corpus const&) = delete;
    Corpus& operator=(Corpus const&) = delete;

    /** Holds the results of visiting the AST.

        This is a table of key/value pairs where
        the key is the SHA1 digest of the USR and
        the value is the bitcode-encoded representation
        of the Info object.
    */
    std::unique_ptr<tooling::ToolResults> toolResults;

    Index Idx;

    /** Table of Info keyed on USR.

        A USRs is a string that provide an
        unambiguous reference to a symbol.
    */
    llvm::StringMap<
        std::unique_ptr<mrdox::Info>> USRToInfo;
};

/** Return a Corpus built using the specified configuration.
*/
Result<Corpus>
buildCorpus(
    Config const& config,
    Reporter& R);

llvm::Error
doMapping(
    Corpus& corpus,
    Config const& cfg);

/** Build the internal index of the program under analysis.

    This must happen before generating docs.
*/
llvm::Error
buildIndex(
    Corpus& corpus,
    Config const& cfg);

} // mrdox
} // clang

#endif
