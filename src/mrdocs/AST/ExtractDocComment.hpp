//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_EXTRACTDOCCOMMENT_HPP
#define MRDOCS_LIB_AST_EXTRACTDOCCOMMENT_HPP

#include <mrdocs/Platform.hpp>
#include "../Diagnostics.hpp"
#include <mrdocs/Config.hpp>
#include <mrdocs/Metadata/DocComment.hpp>

namespace clang {
class ASTContext;
class RawComment;
namespace comments {
class FullComment;
} // comments
} // clang

namespace mrdocs {

/** Initialize clang to recognize our custom comments.

    Safe to be called more than once, but
    not concurrently.
*/
void
initCustomCommentCommands(
    clang::ASTContext& ctx);

/** Extract doc comments from a parsed FullComment.

    Populate the DocComment object with the information
    parsed by Clang. Used both for declaration comments
    (caller passes `someDecl->getASTContext()`) and for
    macro comments, where there is no `Decl`.

    @param jd The DocComment object to populate
    @param FC The full comment to parse
    @param ctx The ASTContext owning the comment
    @param config The MrDocs configuration object
    @param diags The diagnostics object
*/
void
populateDocComment(
    Optional<DocComment>& jd,
    clang::comments::FullComment const* FC,
    clang::ASTContext const& ctx,
    Config const& config,
    Diagnostics& diags);

} // mrdocs


#endif // MRDOCS_LIB_AST_EXTRACTDOCCOMMENT_HPP
