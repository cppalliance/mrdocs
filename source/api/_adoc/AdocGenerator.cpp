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

#include "AdocGenerator.hpp"
#include "AdocMultiPageWriter.hpp"
#include "AdocPagesBuilder.hpp"
#include "AdocSinglePageWriter.hpp"
#include "Support/RawOstream.hpp"
#include "Support/SafeNames.hpp"

namespace clang {
namespace mrdox {
namespace adoc {

//------------------------------------------------
//
// AdocGenerator
//
//------------------------------------------------

Err
AdocGenerator::
build(
    std::string_view outputPath,
    Corpus const& corpus,
    Reporter& R) const
{
    if(corpus.config.singlePage)
        return Generator::build(outputPath, corpus, R);
    return AdocPagesBuilder(
        llvm::StringRef(outputPath), corpus, R).build();
}

Err
AdocGenerator::
buildOne(
    std::ostream& os,
    Corpus const& corpus,
    Reporter& R) const
{
    RawOstream raw_os(os);
    return AdocSinglePageWriter(raw_os, corpus, R).build();
}

} // adoc

//------------------------------------------------

std::unique_ptr<Generator>
makeAdocGenerator()
{
    return std::make_unique<adoc::AdocGenerator>();
}

} // mrdox
} // clang
