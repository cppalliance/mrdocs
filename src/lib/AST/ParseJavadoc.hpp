//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_AST_PARSEJAVADOC_HPP
#define MRDOCS_LIB_AST_PARSEJAVADOC_HPP

#include <mrdocs/Platform.hpp>
#include <lib/Diagnostics.hpp>
#include <mrdocs/Config.hpp>
#include <mrdocs/Metadata/Javadoc.hpp>

namespace clang {
class Decl;
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

/** Parse doc comments from a declaration

    Parse the Javadoc from a declaration, populating the
    Javadoc object with the parsed information.

    @param jd The Javadoc object to populate
    @param FC The full comment to parse
    @param D The declaration to which the comment applies
    @param config The MrDocs configuration object
    @param diags The diagnostics object
*/
void
parseJavadoc(
    Optional<Javadoc>& jd,
    clang::comments::FullComment const* FC,
    clang::Decl const* D,
    Config const& config,
    Diagnostics& diags);

} // mrdocs


#endif // MRDOCS_LIB_AST_PARSEJAVADOC_HPP
