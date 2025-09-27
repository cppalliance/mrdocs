//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_JAVADOC_NODE_NODEKIND_HPP
#define MRDOCS_API_METADATA_JAVADOC_NODE_NODEKIND_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom.hpp>

namespace mrdocs::doc {

/** The kind of node.

    This includes tags and block types.

    Some of the available tags are:

    @li `@author Author Name`
    @li `{@docRoot}`
    @li `@version version`
    @li `@since since-text `
    @li `@see reference`
    @li `@param name description`
    @li `@return description`
    @li `@exception classname description`
    @li `@throws classname description`
    @li `@deprecated description`
    @li `{@inheritDoc}`
    @li `{@link reference}`
    @li `{@linkplain reference}`
    @li `{@value #STATIC_FIELD}`
    @li `{@code literal}`
    @li `{@literal literal}`
    @li `{@serial literal}`
    @li `{@serialData literal}`
    @li `{@serialField literal}`

    Doxygen also introduces a number of additional tags on top
    of the the doc comment specification.

    @note When a new tag is added, the `visit` function overloads
    must be updated to handle the new tag.

    @see https://en.wikipedia.org/wiki/Javadoc[Javadoc - Wikipedia]
    @see https://docs.oracle.com/javase/1.5.0/docs/tooldocs/solaris/javadoc.html[Javadoc Documentation]
    @see https://docs.oracle.com/en/java/javase/13/docs/specs/javadoc/doc-comment-spec.html[Doc Comment Specification]
    @see https://www.oracle.com/java/technologies/javase/javadoc-tool.html[Javadoc Tool]
    @see https://www.oracle.com/technical-resources/articles/java/javadoc-tool.html[How to Write Doc Comments]
    @see https://docs.oracle.com/javase/8/docs/technotes/tools/unix/javadoc.html[Javadoc Package]
    @see https://web.archive.org/web/20170714215721/http://agile.csc.ncsu.edu:80/SEMaterials/tutorials/javadoc[Javadoc Tutorial]
    @see https://en.wikipedia.org/wiki/Doxygen[Doxygen - Wikipedia]
    @see https://www.doxygen.nl/manual/commands.html[Doxygen Special Tags]

 */
enum class NodeKind
{
    // VFALCO Don't forget to update
    // Node::isText() and Node::isBlock()
    // when changing this enum!

    /// A text tag
    text = 1, // needed by bitstream
    /// An admonition tag
    admonition,
    /// A brief tag
    brief,
    /// A code tag
    code,
    /// A heading tag
    heading,
    /// A link tag
    link,
    /// A list_item tag
    list_item,
    /// An unordered_list tag
    unordered_list,
    /// A paragraph tag
    paragraph,
    /// A param tag
    param,
    /// A returns tag
    returns,
    /// A styled tag
    styled,
    /// A tparam tag
    tparam,
    /// A reference tag
    reference,
    /// A copy_details tag
    copy_details,
    /// A throws tag
    throws,
    /// A details tag
    details,
    /// A see tag
    see,
    /// A general tag.
    precondition,
    /// A postcondition tag.
    postcondition
};

/** Return the name of the NodeKind as a string.
 */
MRDOCS_DECL
dom::String
toString(NodeKind kind) noexcept;

/** Return the NodeKind from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    NodeKind const kind)
{
    v = toString(kind);
}

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_JAVADOC_NODE_NODEKIND_HPP
