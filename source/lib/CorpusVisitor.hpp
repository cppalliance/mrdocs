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

//
// This file implements the Mapper piece of the clang-doc tool. It implements
// a RecursiveASTVisitor to look at each declaration and populate the info
// into the internal representation. Each seen declaration is serialized to
// to bitcode and written out to the ExecutionContext as a KV pair where the
// key is the declaration's USR and the value is the serialized bitcode.
//

#ifndef MRDOX_SOURCE_CORPUS_VISITOR_HPP
#define MRDOX_SOURCE_CORPUS_VISITOR_HPP

#include <mrdox/BasicVisitor.hpp>

namespace clang {
namespace mrdox {

/** A visitor which merges tool results into the corpus.
*/
class CorpusVisitor
    : public BasicVisitor
{
    Corpus& corpus_;

public:
    CorpusVisitor(
        Corpus& corpus,
        Config const& cfg) noexcept
        : BasicVisitor(cfg)
        , corpus_(corpus)
    {
    }

private:
    void reportResult(StringRef Key, StringRef Value) override;
};

} // mrdox
} // clang

#endif
