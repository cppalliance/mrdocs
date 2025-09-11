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

#ifndef MRDOCS_API_METADATA_JAVADOC_HPP
#define MRDOCS_API_METADATA_JAVADOC_HPP

#include <algorithm>
#include <memory>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Dom/String.hpp>
#include <mrdocs/Metadata/SymbolID.hpp>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/Concepts.hpp>
#include <mrdocs/Support/Visitor.hpp>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace clang::mrdocs {

/** Javadoc related types and functions.

    Javadoc is a documentation generator originally
    created for the Java language from source code.

    The Javadoc documentation generator tool can interpret
    text in the "doc comments" format included
    directly in the source code.

    The same "doc comments" format has been replicated
    and extended by documentation systems for other
    languages, including the cross-language Doxygen
    and the JSDoc system for JavaScript.

    Because Clang can already parse and extract
    blocks of Javadoc-style comments from source
    code, these classes are used to represent the
    parsed documentation in a structured form.

    @see https://en.wikipedia.org/wiki/Javadoc
    @see https://www.doxygen.nl

 */
namespace doc {

struct Node;

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

/** The text style.
*/
enum class Style
{
    /// No style
    none = 1, // needed by bitstream
    /// Monospaced text
    mono,
    /// Bold text
    bold,
    /// Italic text
    italic
};

/** Return the name of the @ref Style as a string.

    @param kind The style kind.
    @return The string representation of the style.
 */
MRDOCS_DECL
dom::String
toString(Style kind) noexcept;

/** Return the @ref Style from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Style const kind)
{
    v = toString(kind);
}

/** An admonishment style.
*/
enum class Admonish
{
    /// No admonishment
    none = 1, // needed by bitstream
    /// A general note
    note,
    /// A tip to the reader
    tip,
    /// Something important
    important,
    /// A caution admonishment
    caution,
    /// A warning admonishment
    warning
};

/** Return the name of the Admonish as a string.
 */
MRDOCS_DECL
dom::String
toString(Admonish kind) noexcept;

/** Return the Admonish from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Admonish const kind)
{
    v = toString(kind);
}


/** Parameter pass direction.
*/
enum class ParamDirection
{
    /// No direction specified
    none,
    /// Parameter is passed
    in,
    /// Parameter is passed back to the caller
    out,
    /// Parameter is passed and passed back to the caller
    inout
};

/** Return the name of the ParamDirection as a string.
 */
MRDOCS_DECL
dom::String
toString(ParamDirection kind) noexcept;

/** Return the ParamDirection from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ParamDirection const kind)
{
    v = toString(kind);
}

/** Which parts of the documentation to copy.

    @li `all`: copy the brief and the description.
    @li `brief`: only copy the brief.
    @li `description`: only copy the description.
*/
enum class Parts
{
    /// Copy the brief and the description
    all = 1, // needed by bitstream
    /// Copy the brief
    brief,
    /// Copy the description
    description
};

/** Return the name of the Parts as a string.
 */
MRDOCS_DECL
dom::String
toString(Parts kind) noexcept;

/** Return the Parts from a @ref dom::Value string.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Parts const kind)
{
    v = toString(kind);
}

//--------------------------------------------

/** This is a variant-like list element.

    There are two types of nodes: text and block.

    - The javadoc is a list of blocks.
    - A block contains a list of text elements.
    - A text element contains a string.
*/
struct MRDOCS_DECL
    Node
{
    NodeKind Kind;

    virtual ~Node() = default;

    explicit Node(NodeKind const kind_) noexcept
        : Kind(kind_)
    {
    }

    virtual bool isBlock() const noexcept = 0;

    bool isText() const noexcept
    {
        return ! isBlock();
    }

    auto operator<=>(Node const&) const = default;
    bool operator==(Node const&)const noexcept = default;
    virtual bool equals(Node const& other) const noexcept
    {
        return Kind == other.Kind;
    }
};

/** Map the @ref Node to a @ref dom::Object.

    @param io The output parameter to receive the dom::Object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag,
    IO& io,
    Node const& I,
    DomCorpus const* domCorpus)
{
    MRDOCS_ASSERT(domCorpus);
    io.map("kind", I.Kind);
}

/** Return the @ref Node as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Node const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Map the Polymorphic Node as a @ref dom::Value object.

    @param io The output parameter to receive the dom::Object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO, polymorphic_storage_for<Node> NodeTy>
void
tag_invoke(
    dom::ValueFromTag,
    IO& io,
    NodeTy const& I,
    DomCorpus const* domCorpus)
{
    visit(*I, [&](auto const& U)
    {
        tag_invoke(
            dom::ValueFromTag{},
            io,
            U,
            domCorpus);
    });
}

//------------------------------------------------
//
// Text nodes
//
//------------------------------------------------

/** A Node containing a string of text.

    There will be no newlines in the text. Otherwise,
    this would be represented as multiple text nodes
    within a Paragraph node.
*/
struct Text : Node
{
    std::string string;

    static constexpr auto static_kind = NodeKind::text;

    explicit
    Text(
        std::string string_ = std::string()) noexcept
        : Node(NodeKind::text)
        , string(std::move(string_))
    {
    }

    bool
    isBlock() const noexcept final
    {
        return false;
    }

    auto operator<=>(Text const&) const = default;
    bool operator==(Text const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Text const&>(other);
    }

protected:
    Text(
        std::string string_,
        NodeKind kind_)
        : Node(kind_)
        , string(std::move(string_))
    {
    }
};

/** Map the @ref Text to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Text const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Node const&>(I), domCorpus);
    io.map("string", I.string);
}

/** Return the @ref Text as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Text const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

MRDOCS_DECL
std::strong_ordering
operator<=>(Polymorphic<Text> const& lhs, Polymorphic<Text> const& rhs);

inline
bool
operator==(Polymorphic<Text> const& lhs, Polymorphic<Text> const& rhs) {
    return std::is_eq(lhs <=> rhs);
}

/** A piece of styled text.
*/
struct Styled final : Text
{
    Style style;

    static constexpr auto static_kind = NodeKind::styled;

    Styled(
        std::string string_ = std::string(),
        Style style_ = Style::none) noexcept
        : Text(std::move(string_), NodeKind::styled)
        , style(style_)
    {
    }

    auto operator<=>(Styled const&) const = default;
    bool operator==(Styled const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Styled const&>(other);
    }
};

/** Map the @ref Styled to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Styled const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Text const&>(I), domCorpus);
    io.map("style", I.style);
}

/** Return the @ref Styled as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Styled const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}


/** A hyperlink.
*/
struct Link final : Text
{
    std::string href;

    static constexpr auto static_kind = NodeKind::link;

    explicit
    Link(
        std::string string_ = std::string(),
        std::string href_ = std::string()) noexcept
        : Text(std::move(string_), NodeKind::link)
        , href(std::move(href_))
    {
    }

    auto operator<=>(Link const&) const = default;
    bool operator==(Link const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Link const&>(other);
    }
};

/** Map the @ref Link to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Link const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Text const&>(I), domCorpus);
    io.map("href", I.href);
}

/** Return the @ref Link as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Link const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** A reference to a symbol.
*/
struct Reference : Text
{
    SymbolID id = SymbolID::invalid;

    static constexpr auto static_kind = NodeKind::reference;

    explicit
    Reference(
        std::string string_ = std::string()) noexcept
        : Text(std::move(string_), NodeKind::reference)
    {
    }

    auto operator<=>(Reference const&) const = default;
    bool operator==(Reference const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Reference const&>(other);
    }

protected:
    Reference(
        std::string string_,
        NodeKind const kind_) noexcept
        : Text(std::move(string_), kind_)
    {
    }
};

/** Map the @ref Reference to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Reference const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Text const&>(I), domCorpus);
    io.map("symbol", I.id);
}

/** Return the @ref Reference as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Reference const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Documentation copied from another symbol.
*/
struct CopyDetails final : Reference
{
    static constexpr auto static_kind = NodeKind::copy_details;

    CopyDetails(std::string string_ = std::string()) noexcept
        : Reference(std::move(string_), NodeKind::copy_details)
    {
    }

    auto operator<=>(CopyDetails const&) const = default;
    bool operator==(CopyDetails const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<CopyDetails const&>(other);
    }
};

/** Map the @ref CopyDetails to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    CopyDetails const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Reference const&>(I), domCorpus);
}

/** Return the @ref CopyDetails as a @ref dom::Value object.

    @param v The output value.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    CopyDetails const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

//------------------------------------------------
//
// Block nodes
//
//------------------------------------------------

/** A piece of block content

    The top level is a list of blocks.

    There are two types of blocks: headings and paragraphs
*/
struct MRDOCS_DECL
    Block : Node
{
    std::vector<Polymorphic<Text>> children;

    bool isBlock() const noexcept final
    {
        return true;
    }

    bool empty() const noexcept
    {
        return children.empty();
    }

    auto operator<=>(Block const& other) const {
        if (auto const cmp = children.size() <=> other.children.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        for (std::size_t i = 0; i < children.size(); ++i)
        {
            if (auto const cmp = *children[i] <=> *other.children[i];
                !std::is_eq(cmp))
            {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    bool operator==(Block const& other) const noexcept
    {
        if (Kind != other.Kind)
        {
            return false;
        }
        return std::ranges::
            equal(children, other.children,
            [](auto const& a, auto const& b)
            {
                return a->equals(*b);
            });
    }

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Block const&>(other);
    }

    template<std::derived_from<Text> T>
    T& emplace_back(T&& text)
    {
        return static_cast<T&>(emplace_back(
            std::make_unique<T>(std::forward<T>(text))));
    }

    void append(std::vector<Polymorphic<Node>>&& blocks);

    void append(std::vector<Polymorphic<Text>> const& otherChildren);

protected:
    explicit
    Block(
        NodeKind const kind_,
        std::vector<Polymorphic<Text>> children_ = {}) noexcept
        : Node(kind_)
        , children(std::move(children_))
    {
    }

private:
    Text& emplace_back(Polymorphic<Text> text);
};

/** Map the @ref Block to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Block const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Node const&>(I), domCorpus);
    io.defer("children", [&I, domCorpus] {
        return dom::LazyArray(I.children, domCorpus);
    });
}

/** Return the @ref Block as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Block const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** A manually specified section heading.
*/
struct Heading final : Block
{
    static constexpr auto static_kind = NodeKind::heading;

    std::string string;

    Heading(
        std::string string_ = std::string()) noexcept
        : Block(NodeKind::heading)
        , string(std::move(string_))
    {
    }

    auto operator<=>(Heading const&) const = default;
    bool operator==(Heading const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Heading const&>(other);
    }
};

/** Map the @ref Heading to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Heading const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
    io.map("string", I.string);
}

/** Return the @ref Heading as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Heading const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** A sequence of text nodes.
*/
struct Paragraph : Block
{
    static constexpr auto static_kind = NodeKind::paragraph;

    Paragraph() noexcept : Block(NodeKind::paragraph) {}

    virtual
    Paragraph&
    operator=(std::string_view str);

    auto operator<=>(Paragraph const&) const = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Paragraph const&>(other);
    }

protected:
    explicit
    Paragraph(
        NodeKind const kind,
        std::vector<Polymorphic<Text>> children_ = {}) noexcept
        : Block(kind, std::move(children_))
    {
    }
};

/** Map the @ref Paragraph to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Paragraph const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Block const&>(I), domCorpus);
}

/** Return the @ref Paragraph as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Paragraph const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}


/** The brief description
*/
struct Brief final : Paragraph
{
    static constexpr NodeKind static_kind = NodeKind::brief;

    std::vector<std::string> copiedFrom;

    Brief() noexcept
        : Paragraph(NodeKind::brief)
    {
    }

    explicit
    Brief(std::string_view const text)
        : Brief()
    {
        operator=(text);
    }

    Brief(Brief const& other) = default;

    Brief&
    operator=(Brief const& other) = default;

    Brief&
    operator=(std::string_view const text) override
    {
        Paragraph::operator=(text);
        return *this;
    }

    auto operator<=>(Brief const&) const = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Brief const&>(other);
    }
};

/** Map the @ref Brief to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Brief const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref Brief as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Brief const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** An admonition.

    This paragraph represents an admonition, such as
    a note, tip, important, caution, or warning.

*/
struct Admonition final : Paragraph
{
    Admonish admonish;

    explicit
    Admonition(
        Admonish const admonish_ = Admonish::none) noexcept
        : Paragraph(NodeKind::admonition)
        , admonish(admonish_)
    {
    }

    using Paragraph::operator=;

    auto operator<=>(Admonition const&) const = default;

    bool operator==(Admonition const&) const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Admonition const&>(other);
    }
};

/** Map the @ref Admonition to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Admonition const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
    io.map("admonish", I.admonish);
}

/** Return the @ref Admonition as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Admonition const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Preformatted source code.
*/
struct Code final : Paragraph
{
    // VFALCO we can add a language (e.g., C++),
    //        then emit attributes in the generator.

    static constexpr auto static_kind = NodeKind::code;

    Code() noexcept
        : Paragraph(NodeKind::code)
    {
    }

    using Paragraph::operator=;
    auto operator<=>(Code const&) const = default;
    bool operator==(Code const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Code const&>(other);
    }
};

/** Map the @ref Code to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Code const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref Code as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Code const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** An item in a list
*/
struct ListItem final : Paragraph
{
    static constexpr auto static_kind = NodeKind::list_item;

    ListItem()
        : Paragraph(static_kind)
    {}

    using Paragraph::operator=;

    auto operator<=>(ListItem const&) const = default;
    bool
    operator==(ListItem const&) const noexcept = default;

    bool
    equals(Node const& other) const noexcept override
    {
        auto* p = dynamic_cast<ListItem const*>(&other);
        if (!p)
        {
            return false;
        }
        if (this == &other)
        {
            return true;
        }
        return *this == *p;
    }
};

/** Map the @ref ListItem to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    ListItem const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref ListItem as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    ListItem const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** A list of list items
*/
struct UnorderedList final : Paragraph
{
    static constexpr auto static_kind = NodeKind::unordered_list;

    std::vector<ListItem> items;

    UnorderedList()
        : Paragraph(static_kind)
    {
    }

    using Paragraph::operator=;

    auto operator<=>(UnorderedList const& other) const {
        if (auto const cmp = items.size() <=> other.items.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (auto const cmp = items[i] <=> other.items[i];
                !std::is_eq(cmp))
            {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }

    bool
    operator==(UnorderedList const&) const noexcept = default;

    bool
    equals(Node const& other) const noexcept override
    {
        auto* p = dynamic_cast<UnorderedList const*>(&other);
        if (!p)
        {
            return false;
        }
        if (this == &other)
        {
            return true;
        }
        return *this == *p;
    }
};

/** Map the @ref UnorderedList to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    UnorderedList const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
    io.defer("items", [&I, domCorpus] {
        return dom::LazyArray(I.items, domCorpus);
    });
}

/** Return the @ref UnorderedList as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    UnorderedList const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** A @see paragraph
*/
struct See final : Paragraph
{
    static constexpr auto static_kind = NodeKind::see;

    See()
        : Paragraph(static_kind)
    {
    }

    using Paragraph::operator=;

    auto operator<=>(See const&) const = default;

    bool operator==(See const&)
        const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<See const&>(other);
    }
};

/** Map the @ref See to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    See const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref See as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    See const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Documentation for a function parameter
*/
struct Param final : Paragraph
{
    std::string name;
    ParamDirection direction;

    static constexpr auto static_kind = NodeKind::param;

    Param(
        std::string name_ = std::string(),
        Paragraph details_ = Paragraph(),
        ParamDirection const direction_ = ParamDirection::none)
        : Paragraph(
            NodeKind::param,
            std::move(details_.children))
        , name(std::move(name_))
        , direction(direction_)
    {
    }

    explicit
    Param(
        std::string_view const name,
        std::string_view const text)
    : Param()
    {
        this->name = name;
        this->operator=(text);
    }

    explicit
    Param(Paragraph const& other)
        : Param()
    {
        children = other.children;
    }

    Param&
    operator=(std::string_view const text) override
    {
        Paragraph::operator=(text);
        return *this;
    }

    auto operator<=>(Param const&) const = default;

    bool operator==(Param const&)
        const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Param const&>(other);
    }
};

/** Map the @ref Param to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Param const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
    io.map("name", I.name);
    io.map("direction", I.direction);
}

/** Return the @ref Param as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Param const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Documentation for a function return type
*/
struct Returns final : Paragraph
{
    static constexpr NodeKind static_kind = NodeKind::returns;

    Returns()
        : Paragraph(NodeKind::returns)
    {
    }

    explicit
    Returns(std::string_view const text)
        : Returns()
    {
        operator=(text);
    }

    explicit
    Returns(Paragraph const& other)
        : Returns()
    {
        children = other.children;
    }

    Returns&
    operator=(std::string_view const text) override
    {
        Paragraph::operator=(text);
        return *this;
    }

    auto operator<=>(Returns const&) const = default;

    bool operator==(Returns const&)
        const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Returns const&>(other);
    }
};

/** Map the @ref Returns to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Returns const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref Returns as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Returns const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Documentation for a template parameter
*/
struct TParam final : Paragraph
{
    std::string name;

    static constexpr NodeKind static_kind = NodeKind::tparam;

    TParam()
        : Paragraph(NodeKind::tparam)
    {
    }

    using Paragraph::operator=;

    auto operator<=>(TParam const&) const = default;
    bool operator==(TParam const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<TParam const&>(other);
    }
};

/** Map the @ref TParam to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    TParam const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
    io.map("name", I.name);
}

/** Return the @ref TParam as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    TParam const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

/** Documentation for a function parameter
*/
struct Throws final : Paragraph
{
    Reference exception;

    static constexpr NodeKind static_kind = NodeKind::throws;

    Throws(
        std::string exception_ = std::string(),
        Paragraph details_ = Paragraph())
        : Paragraph(
            NodeKind::throws,
            std::move(details_.children))
        , exception(std::move(exception_))
    {
    }

    using Paragraph::operator=;

    auto operator<=>(Throws const&) const = default;

    bool operator==(Throws const&)
        const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Throws const&>(other);
    }
};

/** Map the @ref Throws to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Throws const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
    io.map("exception", I.exception);
}

/** Return the @ref Throws as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Throws const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

struct Precondition final : Paragraph
{
    static constexpr NodeKind static_kind = NodeKind::precondition;

    Precondition(
        Paragraph details_ = Paragraph())
        : Paragraph(
            NodeKind::precondition,
            std::move(details_.children))
    {
    }

    using Paragraph::operator=;

    auto operator<=>(Precondition const&) const = default;

    bool operator==(Precondition const&)
        const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Precondition const&>(other);
    }
};

/** Map the @ref Precondition to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Precondition const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref Precondition as a @ref dom::Value object.

    @param v The value to assign to.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Precondition const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

struct Postcondition : Paragraph
{
    static constexpr auto static_kind = NodeKind::postcondition;

    Postcondition(
        Paragraph details_ = Paragraph())
        : Paragraph(
            NodeKind::postcondition,
            std::move(details_.children))
    {
    }

    using Paragraph::operator=;

    auto operator<=>(Postcondition const&) const = default;

    bool operator==(Postcondition const&)
        const noexcept = default;

    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Postcondition const&>(other);
    }
};

/** Map the @ref Postcondition to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Postcondition const& I,
    DomCorpus const* domCorpus)
{
    tag_invoke(t, io, dynamic_cast<Paragraph const&>(I), domCorpus);
}

/** Return the @ref Postcondition as a @ref dom::Value object.

    @param v The value to assign to.
    @param I The input object.
    @param domCorpus The DOM corpus, or nullptr if not part of a corpus.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Postcondition const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

//------------------------------------------------

/** Visit a node.

    @param node The node to visit.
    @param fn The function to call for each node.
    @param args Additional arguments to pass to the function.
    @return The result of calling the function.
 */
template<
    class NodeTy,
    class Fn,
    class... Args>
    requires std::derived_from<NodeTy, Node>
decltype(auto)
visit(
    NodeTy& node,
    Fn&& fn,
    Args&&... args)
{
    auto visitor = makeVisitor<Node>(
        node, std::forward<Fn>(fn),
        std::forward<Args>(args)...);
    switch(node.Kind)
    {
    case NodeKind::admonition:
        return visitor.template visit<Admonition>();
    case NodeKind::brief:
        return visitor.template visit<Brief>();
    case NodeKind::code:
        return visitor.template visit<Code>();
    case NodeKind::heading:
        return visitor.template visit<Heading>();
    case NodeKind::paragraph:
        return visitor.template visit<Paragraph>();
    case NodeKind::link:
        return visitor.template visit<Link>();
    case NodeKind::reference:
        return visitor.template visit<Reference>();
    case NodeKind::copy_details:
        return visitor.template visit<CopyDetails>();
    case NodeKind::list_item:
        return visitor.template visit<ListItem>();
    case NodeKind::unordered_list:
        return visitor.template visit<UnorderedList>();
    case NodeKind::param:
        return visitor.template visit<Param>();
    case NodeKind::returns:
        return visitor.template visit<Returns>();
    case NodeKind::styled:
        return visitor.template visit<Styled>();
    case NodeKind::text:
        return visitor.template visit<Text>();
    case NodeKind::tparam:
        return visitor.template visit<TParam>();
    case NodeKind::throws:
        return visitor.template visit<Throws>();
    case NodeKind::see:
        return visitor.template visit<See>();
    case NodeKind::precondition:
        return visitor.template visit<Precondition>();
    case NodeKind::postcondition:
        return visitor.template visit<Postcondition>();
    default:
        MRDOCS_UNREACHABLE();
    }
}

/** Traverse a list of nodes.

    @param list The list of nodes to traverse.
    @param f The function to call for each node.
    @param args Additional arguments to pass to the function.
 */
template<class F, class T, class... Args>
requires std::derived_from<T, Node>
void traverse(
    std::vector<std::unique_ptr<T>> const& list,
    F&& f, Args&&... args)
{
    for(auto const& node : list)
        visit(*node,
            std::forward<F>(f),
            std::forward<Args>(args)...);
}

} // doc

//------------------------------------------------

class Corpus;

/** A processed Doxygen-style comment attached to a declaration.
*/
struct MRDOCS_DECL
    Javadoc
{
    /// The list of text blocks.
    std::vector<Polymorphic<doc::Block>> blocks;

    // ----------------------
    // Symbol Metadata

    /// A brief description of the symbol.
    std::optional<doc::Brief> brief;

    /** The list of return type descriptions.

        Multiple return descriptions are allowed.

        The results are concatenated in the order
        they appear in the source code.
     */
    std::vector<doc::Returns> returns;

    /// The list of parameters.
    std::vector<doc::Param> params;

    /// The list of template parameters.
    std::vector<doc::TParam> tparams;

    /// The list of exceptions.
    std::vector<doc::Throws> exceptions;

    /// The list of "see also" references.
    std::vector<doc::See> sees;

    /// The list of preconditions.
    std::vector<doc::Precondition> preconditions;

    /// The list of postconditions.
    std::vector<doc::Postcondition> postconditions;

    /** The list of "relates" references.

        These references are creates with the
        \\relates command.
     */
    std::vector<doc::Reference> relates;

    /** The list of "related" references.

        These references are the inverse of
        the \\relates command.
     */
    std::vector<doc::Reference> related;

    /** Constructor.
    */
    MRDOCS_DECL
    Javadoc() noexcept;

    /** Constructor
    */
    explicit
    Javadoc(
        std::vector<Polymorphic<doc::Block>> blocks);

    /** Return true if this is empty
    */
    bool
    empty() const noexcept
    {
        return
            blocks.empty() &&
            !brief &&
            returns.empty() &&
            params.empty() &&
            tparams.empty() &&
            exceptions.empty() &&
            sees.empty() &&
            related.empty() &&
            preconditions.empty() &&
            postconditions.empty();
    }

    /** Return the list of top level blocks.
    */
    std::vector<Polymorphic<doc::Block>> const&
    getBlocks() const noexcept
    {
        return blocks;
    }

    // VFALCO This is unfortunately necessary for
    //        the deserialization from bitcode...
    std::vector<Polymorphic<doc::Block>>&
    getBlocks() noexcept
    {
        return blocks;
    }

    //--------------------------------------------

    /** Comparison

        These are used internally to impose a
        total ordering, and not visible in the
        output format.
    */
    /** @{ */
    auto operator<=>(Javadoc const& other) const noexcept {
        if (auto const cmp = blocks.size() <=> other.blocks.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        for (std::size_t i = 0; i < blocks.size(); ++i)
        {
            if (auto cmp = CompareDerived(blocks[i], other.blocks[i]);
                !std::is_eq(cmp))
            {
                return cmp;
            }
        }
        return std::strong_ordering::equal;
    }
    bool operator==(Javadoc const&) const noexcept;
    bool operator!=(Javadoc const&) const noexcept;
    /* @} */

    //--------------------------------------------

    /** Attempt to append a block.

        @return An empty string on success, otherwise
        a string indicating the reason for the failure.

        @param block The block to append.
    */
    template<std::derived_from<doc::Block> T>
    std::string
    emplace_back(T&& block)
    {
        return emplace_back(Polymorphic<doc::Block>(std::in_place_type<T>,
                                                    std::forward<T>(block)));
    }

    /** Append blocks from another javadoc to this.
    */
    void append(Javadoc&& other);

    /** Append blocks from a list to this.

        @param blocks The blocks to append.
     */
    void
    append(std::vector<Polymorphic<doc::Node>>&& blocks);

private:
    std::string
    emplace_back(Polymorphic<doc::Block>);
};

inline
void merge(Javadoc& I, Javadoc&& other)
{
    // FIXME: this doesn't merge parameter information;
    // parameters with the same name but different direction
    // or descriptions end up being duplicated
    if(other != I)
    {
        // Unconditionally extend the blocks
        // since each decl may have a comment.
        I.append(std::move(other));
    }
}

/** Map the @ref Javadoc to a @ref dom::Object.

    @param t The tag.
    @param io The output object.
    @param I The javadoc to map.
    @param domCorpus The DOM corpus, or nullptr.
 */
template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    Javadoc const& I,
    DomCorpus const* domCorpus)
{
    io.defer("description", [&I, domCorpus] {
        return dom::LazyArray(I.blocks, domCorpus);
    });
    if (I.brief && !I.brief->children.empty())
    {
        io.map("brief", I.brief);
    }
    io.defer("returns", [&I, domCorpus] {
        return dom::LazyArray(I.returns, domCorpus);
    });
    io.defer("params", [&I, domCorpus] {
        return dom::LazyArray(I.params, domCorpus);
    });
    io.defer("tparams", [&I, domCorpus] {
        return dom::LazyArray(I.tparams, domCorpus);
    });
    io.defer("exceptions", [&I, domCorpus] {
        return dom::LazyArray(I.exceptions, domCorpus);
    });
    io.defer("sees", [&I, domCorpus] {
        return dom::LazyArray(I.sees, domCorpus);
    });
    io.defer("relates", [&I, domCorpus] {
        return dom::LazyArray(I.relates, domCorpus);
    });
    io.defer("related", [&I, domCorpus] {
        return dom::LazyArray(I.related, domCorpus);
    });
    io.defer("preconditions", [&I, domCorpus] {
        return dom::LazyArray(I.preconditions, domCorpus);
    });
    io.defer("postconditions", [&I, domCorpus] {
        return dom::LazyArray(I.postconditions, domCorpus);
    });
}

/** Return the @ref Javadoc as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Javadoc const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

} // clang::mrdocs

#endif
