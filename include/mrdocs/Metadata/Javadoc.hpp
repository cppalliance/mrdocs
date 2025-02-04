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

#include <mrdocs/Platform.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <mrdocs/Support/Visitor.hpp>
#include <mrdocs/ADT/Polymorphic.hpp>
#include <mrdocs/Dom/String.hpp>
#include <memory>
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

    text = 1, // needed by bitstream
    admonition,
    brief,
    code,
    heading,
    link,
    list_item,
    unordered_list,
    paragraph,
    param,
    returns,
    styled,
    tparam,
    reference,
    copied,
    throws,
    details,
    see,
    precondition,
    postcondition
};

/** A text style.
*/
enum class Style
{
    none = 1, // needed by bitstream
    mono,
    bold,
    italic
};

/** An admonishment style.
*/
enum class Admonish
{
    none = 1, // needed by bitstream
    note,
    tip,
    important,
    caution,
    warning
};

/** Parameter pass direction.
*/
enum class ParamDirection
{
    none,
    in,
    out,
    inout
};

/** Which parts of the documentation to copy.

    @li `all`: copy the brief and the description.
    @li `brief`: only copy the brief.
    @li `description`: only copy the description.
*/
enum class Parts
{
    all = 1, // needed by bitstream
    brief,
    description
};

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

    static constexpr NodeKind static_kind = NodeKind::text;

    explicit
    Text(
        std::string string_ = std::string()) noexcept
        : Node(NodeKind::text)
        , string(std::move(string_))
    {
    }

    bool isBlock() const noexcept final
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

/** Documentation copied from another symbol.
*/
struct Copied final : Reference
{
    Parts parts;

    static constexpr auto static_kind = NodeKind::copied;

    Copied(
        std::string string_ = std::string(),
        Parts parts_ = Parts::all) noexcept
        : Reference(std::move(string_), NodeKind::copied)
        , parts(parts_)
    {
    }

    auto operator<=>(Copied const&) const = default;
    bool operator==(Copied const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Copied const&>(other);
    }
};

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

    Block(Block&&);
    Block(const Block&);
    ~Block();

    Block &operator=(Block&&);
    Block &operator=(const Block&);

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
        return std::equal(children.begin(), children.end(),
            other.children.begin(), other.children.end(),
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

/** A sequence of text nodes.
*/
struct Paragraph : Block
{
    static constexpr auto static_kind = NodeKind::paragraph;

    Paragraph() noexcept
        : Block(NodeKind::paragraph)
    {
    }

    auto operator<=>(Paragraph const&) const = default;
    bool operator==(Paragraph const&) const noexcept = default;
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

/** The brief description
*/
struct Brief final : Paragraph
{
    static constexpr NodeKind static_kind = NodeKind::brief;

    Brief() noexcept
        : Paragraph(NodeKind::brief)
    {
    }

    auto operator<=>(Brief const&) const = default;
    bool operator==(Brief const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Brief const&>(other);
    }
};

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

    auto operator<=>(Admonition const&) const = default;
    bool operator==(Admonition const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Admonition const&>(other);
    }
};

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

    auto operator<=>(Code const&) const = default;
    bool operator==(Code const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<Code const&>(other);
    }
};

/** An item in a list
*/
struct ListItem final : Paragraph
{
    static constexpr auto static_kind = NodeKind::list_item;

    ListItem()
        : Paragraph(NodeKind::list_item)
    {}

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

/** A list of list items
*/
struct UnorderedList final : Paragraph
{
    static constexpr auto static_kind = NodeKind::unordered_list;

    std::vector<ListItem> items;

    UnorderedList()
        : Paragraph(NodeKind::unordered_list)
    {
    }

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
        auto* p = dynamic_cast<const UnorderedList*>(&other);
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

/** A @details paragraph
*/
struct Details final : Paragraph
{
    static constexpr auto static_kind = NodeKind::details;

    Details()
        : Paragraph(NodeKind::details)
    {}

    auto operator<=>(Details const&) const = default;

    bool
    operator==(const Details&) const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        auto* p = dynamic_cast<const Details*>(&other);
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

/** A @see paragraph
*/
struct See final : Paragraph
{
    static constexpr auto static_kind = NodeKind::see;

    See()
        : Paragraph(NodeKind::see)
    {
    }

    auto operator<=>(See const&) const = default;

    bool operator==(const See&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<const See&>(other);
    }
};

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

    auto operator<=>(Param const&) const = default;

    bool operator==(const Param&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<const Param&>(other);
    }
};


/** Documentation for a function return type
*/
struct Returns final : Paragraph
{
    static constexpr NodeKind static_kind = NodeKind::returns;

    Returns()
        : Paragraph(NodeKind::returns)
    {
    }

    auto operator<=>(Returns const&) const = default;

    bool operator==(const Returns&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<const Returns&>(other);
    }
};

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

    auto operator<=>(TParam const&) const = default;
    bool operator==(const TParam&) const noexcept = default;
    bool equals(const Node& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<const TParam&>(other);
    }
};

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

    auto operator<=>(Throws const&) const = default;

    bool operator==(const Throws&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<const Throws&>(other);
    }
};

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

    auto operator<=>(Precondition const&) const = default;

    bool operator==(const Precondition&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<const Precondition&>(other);
    }
};

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

    auto operator<=>(Postcondition const&) const = default;

    bool operator==(const Postcondition&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return Kind == other.Kind &&
            *this == dynamic_cast<const Postcondition&>(other);
    }
};

//------------------------------------------------

/** A visitor for node types.
 */
template<class F, class... Args>
constexpr
auto
visit(
    NodeKind kind,
    F&& f, Args&&... args)
{
    switch(kind)
    {
    case NodeKind::admonition:
        return f.template operator()<Admonition>(std::forward<Args>(args)...);
    case NodeKind::brief:
        return f.template operator()<Brief>(std::forward<Args>(args)...);
    case NodeKind::code:
        return f.template operator()<Code>(std::forward<Args>(args)...);
    case NodeKind::heading:
        return f.template operator()<Heading>(std::forward<Args>(args)...);
    case NodeKind::link:
        return f.template operator()<Link>(std::forward<Args>(args)...);
    case NodeKind::reference:
        return f.template operator()<Reference>(std::forward<Args>(args)...);
    case NodeKind::copied:
        return f.template operator()<Copied>(std::forward<Args>(args)...);
    case NodeKind::list_item:
        return f.template operator()<ListItem>(std::forward<Args>(args)...);
    case NodeKind::unordered_list:
        return f.template operator()<UnorderedList>(std::forward<Args>(args)...);
    case NodeKind::paragraph:
        return f.template operator()<Paragraph>(std::forward<Args>(args)...);
    case NodeKind::param:
        return f.template operator()<Param>(std::forward<Args>(args)...);
    case NodeKind::returns:
        return f.template operator()<Returns>(std::forward<Args>(args)...);
    case NodeKind::styled:
        return f.template operator()<Styled>(std::forward<Args>(args)...);
    case NodeKind::text:
        return f.template operator()<Text>(std::forward<Args>(args)...);
    case NodeKind::tparam:
        return f.template operator()<TParam>(std::forward<Args>(args)...);
    case NodeKind::throws:
        return f.template operator()<Throws>(std::forward<Args>(args)...);
    case NodeKind::details:
        return f.template operator()<Details>(std::forward<Args>(args)...);
    case NodeKind::see:
        return f.template operator()<See>(std::forward<Args>(args)...);
    case NodeKind::precondition:
        return f.template operator()<Precondition>(std::forward<Args>(args)...);
    case NodeKind::postcondition:
        return f.template operator()<Postcondition>(std::forward<Args>(args)...);
    default:
        return f.template operator()<void>(std::forward<Args>(args)...);
    }
}

/** Visit a node.

    @param node The node to visit.
    @param fn The function to call for each node.
    @param args Additional arguments to pass to the function.
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
    case NodeKind::copied:
        return visitor.template visit<Copied>();
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
    case NodeKind::details:
        return visitor.template visit<Details>();
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

struct Overview
{
    std::shared_ptr<Paragraph> brief = nullptr;
    std::vector<Block const*> blocks;
    Returns const* returns = nullptr;
    std::vector<Param const*> params;
    std::vector<TParam const*> tparams;
    std::vector<Throws const*> exceptions;
    std::vector<See const*> sees;
    std::vector<Precondition const*> preconditions;
    std::vector<Postcondition const*> postconditions;
};

MRDOCS_DECL
dom::String
toString(Style style) noexcept;

} // doc

//------------------------------------------------

class Corpus;

/** A processed Doxygen-style comment attached to a declaration.
*/
struct MRDOCS_DECL
    Javadoc
{
    std::vector<Polymorphic<doc::Block>> blocks_;

    /** Constructor.
    */
    Javadoc() noexcept;

    /** Constructor
    */
    explicit
    Javadoc(
        std::vector<Polymorphic<doc::Block>> blocks);

    Javadoc(const Javadoc&);
    Javadoc(Javadoc&&);
    Javadoc& operator=(const Javadoc&);
    Javadoc& operator=(Javadoc&&);

    /** Return true if this is empty
    */
    bool
    empty() const noexcept
    {
        return blocks_.empty();
    }

    /** Return the brief, or nullptr if there is none.
    */
    doc::Paragraph const*
    getBrief(Corpus const& corpus) const noexcept;

    std::vector<Polymorphic<doc::Block>> const&
    getDescription(Corpus const& corpus) const noexcept;

    /** Return the list of top level blocks.
    */
    std::vector<Polymorphic<doc::Block>> const&
    getBlocks() const noexcept
    {
        return blocks_;
    }

    // VFALCO This is unfortunately necessary for
    //        the deserialization from bitcode...
    std::vector<Polymorphic<doc::Block>>&
    getBlocks() noexcept
    {
        return blocks_;
    }

    //--------------------------------------------

    /** Comparison

        These are used internally to impose a
        total ordering, and not visible in the
        output format.
    */
    /** @{ */
    auto operator<=>(Javadoc const& other) const noexcept {
        if (auto const cmp = blocks_.size() <=> other.blocks_.size();
            !std::is_eq(cmp))
        {
            return cmp;
        }
        for (std::size_t i = 0; i < blocks_.size(); ++i)
        {
            if (auto cmp = CompareDerived(blocks_[i], other.blocks_[i]);
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

    /** Return an overview of the javadoc.

        The Javadoc is stored as a list of blocks,
        in the order of appearance in the corresponding
        source code. This function separates elements
        according to their semantic content and returns
        the result as a set of collated lists and
        individual elements.

        Ownership of the nodes is not transferred;
        the returend overview is invalidated if the
        javadoc object is destroyed.
    */
    doc::Overview
    makeOverview(const Corpus& corpus) const;

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
        return emplace_back(MakePolymorphic<doc::Block, T>(std::forward<T>(block)));
    }

    /** Append blocks from another javadoc to this.
    */
    void append(Javadoc&& other);

    /** Append blocks from a list to this.
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


/** Return the Javadoc as a @ref dom::Value.
 */
MRDOCS_DECL
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Javadoc const& I,
    DomCorpus const* domCorpus);

} // clang::mrdocs

#endif
