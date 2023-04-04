//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include "TestAction.hpp"
#include <mrdox/Config.hpp>

namespace clang {
namespace mrdox {

void
TestAction::
EndSourceFileAction()
{
    Corpus corpus;
    if(! R_.success(buildIndex(corpus, results_, cfg_)))
        return;
#if 0
bool
renderCodeAsXML(
    std::string& xml,
    llvm::StringRef cppCode,
    Config const& cfg)
{
    std::unique_ptr<ASTUnit> astUnit =
        clang::tooling::buildASTFromCodeWithArgs(cppCode, {});
    Corpus corpus;
    CorpusVisitor visitor(corpus, cfg);
    visitor.HandleTranslationUnit(astUnit->getASTContext());
    if(llvm::Error err = buildIndex(corpus, cfg))
        return ! err;
    bool success = XMLGenerator(cfg).render(xml, corpus, cfg);
    // VFALCO oops, cfg.Executor->getToolResults()
    //              holds information from the previous corpus...
    //cfg.Executor->getToolResults()->
    return success;
}
#endif
}

} // mrdox
} // clang

