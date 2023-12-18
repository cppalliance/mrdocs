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
#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/Symbols.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Visitor.hpp>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace clang {
namespace mrdocs {

// This namespace contains all of the Javadoc
// related types, constants, and functions.
namespace doc {

struct Node;

using String = std::string;

template<typename T>
requires std::derived_from<T, doc::Node>
using List = std::vector<std::unique_ptr<T>>;

enum class Kind
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
    paragraph,
    param,
    returns,
    styled,
    tparam,
    reference,
    copied,
    throws
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
*/
struct MRDOCS_DECL
    Node
{
    Kind kind;

    virtual ~Node() = default;

    explicit Node(Kind kind_) noexcept
        : kind(kind_)
    {
    }

    virtual bool isBlock() const noexcept = 0;

    bool isText() const noexcept
    {
        return ! isBlock();
    }

    bool operator==(const Node&)const noexcept = default;
    virtual bool equals(const Node& other) const noexcept
    {
        return kind == other.kind;
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
    String string;

    static constexpr Kind static_kind = Kind::text;

    explicit
    Text(
        String string_ = String()) noexcept
        : Node(Kind::text)
        , string(std::move(string_))
    {
    }

    bool isBlock() const noexcept final override
    {
        return false;
    }

    bool operator==(const Text&) const noexcept = default;
    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Text&>(other);
    }

protected:
    Text(
        String string_,
        Kind kind_)
        : Node(kind_)
        , string(std::move(string_))
    {
    }
};

/** A piece of styled text.
*/
struct Styled : Text
{
    Style style;

    static constexpr Kind static_kind = Kind::styled;

    Styled(
        String string_ = String(),
        Style style_ = Style::none) noexcept
        : Text(std::move(string_), Kind::styled)
        , style(style_)
    {
    }

    bool operator==(Styled const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Styled&>(other);
    }

};

/** A hyperlink.
*/
struct Link : Text
{
    String href;

    static constexpr Kind static_kind = Kind::link;

    explicit
    Link(
        String string_ = String(),
        String href_ = String()) noexcept
        : Text(std::move(string_), Kind::link)
        , href(std::move(href_))
    {
    }

    bool operator==(const Link&) const noexcept = default;
    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Link&>(other);
    }
};

/** A reference to a symbol.
*/
struct Reference : Text
{
    SymbolID id = SymbolID::invalid;

    static constexpr Kind static_kind = Kind::reference;

    explicit
    Reference(
        String string_ = String()) noexcept
        : Text(std::move(string_), Kind::reference)
    {
    }

    bool operator==(const Reference&) const noexcept = default;
    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Reference&>(other);
    }

protected:
    Reference(
        String string_,
        Kind kind_) noexcept
        : Text(std::move(string_), kind_)
    {
    }
};

/** Documentation copied from another symbol.
*/
struct Copied : Reference
{
    Parts parts;

    static constexpr Kind static_kind = Kind::copied;

    Copied(
        String string_ = String(),
        Parts parts_ = Parts::all) noexcept
        : Reference(std::move(string_), Kind::copied)
        , parts(parts_)
    {
    }

    bool operator==(Copied const&) const noexcept = default;
    bool equals(Node const& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Copied&>(other);
    }
};

//------------------------------------------------
//
// Block nodes
//
//------------------------------------------------

/** A piece of block content

    The top level is a list of blocks.
*/
struct MRDOCS_DECL
    Block : Node
{
    List<Text> children;

    bool isBlock() const noexcept final override
    {
        return true;
    }

    bool empty() const noexcept
    {
        return children.empty();
    }

    bool operator==(const Block& other) const noexcept
    {
        if(kind != other.kind)
            return false;
        return std::equal(children.begin(), children.end(),
            other.children.begin(), other.children.end(),
            [](const auto& a, const auto& b)
            {
                return a->equals(*b);
            });
    }

    bool equals(Node const& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Block&>(other);
    }

    template<std::derived_from<Text> T>
    T& emplace_back(T&& text)
    {
        return static_cast<T&>(emplace_back(
            std::make_unique<T>(std::move(text))));
    }

    void append(List<Node>&& blocks);

protected:
    explicit
    Block(
        Kind kind_,
        List<Text> children_ = {}) noexcept
        : Node(kind_)
        , children(std::move(children_))
    {
    }

private:
    Text& emplace_back(std::unique_ptr<Text> text);
};

/** A manually specified section heading.
*/
struct Heading : Block
{
    static constexpr Kind static_kind = Kind::heading;

    String string;

    Heading(
        String string_ = String()) noexcept
        : Block(Kind::heading)
        , string(std::move(string_))
    {
    }

    bool operator==(Heading const&) const noexcept = default;
    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Heading&>(other);
    }
};

/** A sequence of text nodes.
*/
struct Paragraph : Block
{
    static constexpr Kind static_kind = Kind::paragraph;

    Paragraph() noexcept
        : Block(Kind::paragraph)
    {
    }

    bool operator==(const Paragraph&) const noexcept = default;
    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Paragraph&>(other);
    }

protected:
    explicit
    Paragraph(
        Kind kind,
        List<Text> children_ = {}) noexcept
        : Block(kind, std::move(children_))
    {
    }
};

/** The brief description
*/
struct Brief : Paragraph
{
    static constexpr Kind static_kind = Kind::brief;

    Brief() noexcept
        : Paragraph(Kind::brief)
    {
    }

    bool operator==(const Brief&) const noexcept = default;
    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Brief&>(other);
    }
};

/** An admonition.
*/
struct Admonition : Paragraph
{
    Admonish admonish;

    explicit
    Admonition(
        Admonish admonish_ = Admonish::none) noexcept
        : Paragraph(Kind::admonition)
        , admonish(admonish_)
    {
    }

    bool operator==(const Admonition&) const noexcept = default;
    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Admonition&>(other);
    }
};

/** Preformatted source code.
*/
struct Code : Paragraph
{
    // VFALCO we can add a language (e.g. C++),
    //        then emit attributes in the generator.

    static constexpr Kind static_kind = Kind::code;

    Code() noexcept
        : Paragraph(Kind::code)
    {
    }

    bool operator==(const Code&) const noexcept = default;
    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Code&>(other);
    }
};

/** An item in a list
*/
struct ListItem : Paragraph
{
    static constexpr Kind static_kind = Kind::list_item;

    ListItem()
        : Paragraph(Kind::list_item)
    {
    }

    bool operator==(const ListItem&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const ListItem&>(other);
    }
};

/** Documentation for a function parameter
*/
struct Param : Paragraph
{
    String name;
    ParamDirection direction;

    static constexpr Kind static_kind = Kind::param;

    Param(
        String name_ = String(),
        Paragraph details_ = Paragraph(),
        ParamDirection direction_ = ParamDirection::none)
        : Paragraph(
            Kind::param,
            std::move(details_.children))
        , name(std::move(name_))
        , direction(direction_)
    {
    }

    bool operator==(const Param&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Param&>(other);
    }
};


/** Documentation for a function return type
*/
struct Returns : Paragraph
{
    static constexpr Kind static_kind = Kind::returns;

    Returns()
        : Paragraph(Kind::returns)
    {
    }

    bool operator==(const Returns&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Returns&>(other);
    }
};
/** Documentation for a template parameter
*/
struct TParam : Paragraph
{
    String name;

    static constexpr Kind static_kind = Kind::tparam;

    TParam()
        : Paragraph(Kind::tparam)
    {
    }

    bool operator==(const TParam&) const noexcept = default;
    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const TParam&>(other);
    }
};

/** Documentation for a function parameter
*/
struct Throws : Paragraph
{
    String exception;

    static constexpr Kind static_kind = Kind::throws;

    Throws(
        String exception_ = String(),
        Paragraph details_ = Paragraph())
        : Paragraph(
            Kind::throws,
            std::move(details_.children))
        , exception(std::move(exception_))
    {
    }

    bool operator==(const Throws&)
        const noexcept = default;

    bool equals(const Node& other) const noexcept override
    {
        return kind == other.kind &&
            *this == static_cast<const Throws&>(other);
    }
};

//------------------------------------------------

template<class F, class... Args>
constexpr
auto
visit(
    Kind kind,
    F&& f, Args&&... args)
{
    switch(kind)
    {
    case Kind::admonition:
        return f.template operator()<Admonition>(std::forward<Args>(args)...);
    case Kind::brief:
        return f.template operator()<Brief>(std::forward<Args>(args)...);
    case Kind::code:
        return f.template operator()<Code>(std::forward<Args>(args)...);
    case Kind::heading:
        return f.template operator()<Heading>(std::forward<Args>(args)...);
    case Kind::link:
        return f.template operator()<Link>(std::forward<Args>(args)...);
    case Kind::reference:
        return f.template operator()<Reference>(std::forward<Args>(args)...);
    case Kind::copied:
        return f.template operator()<Copied>(std::forward<Args>(args)...);
    case Kind::list_item:
        return f.template operator()<ListItem>(std::forward<Args>(args)...);
    case Kind::paragraph:
        return f.template operator()<Paragraph>(std::forward<Args>(args)...);
    case Kind::param:
        return f.template operator()<Param>(std::forward<Args>(args)...);
    case Kind::returns:
        return f.template operator()<Returns>(std::forward<Args>(args)...);
    case Kind::styled:
        return f.template operator()<Styled>(std::forward<Args>(args)...);
    case Kind::text:
        return f.template operator()<Text>(std::forward<Args>(args)...);
    case Kind::tparam:
        return f.template operator()<TParam>(std::forward<Args>(args)...);
    case Kind::throws:
        return f.template operator()<Throws>(std::forward<Args>(args)...);
    default:
        return f.template operator()<void>(std::forward<Args>(args)...);
    }
}

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
    switch(node.kind)
    {
    case Kind::admonition:
        return visitor.template visit<Admonition>();
    case Kind::brief:
        return visitor.template visit<Brief>();
    case Kind::code:
        return visitor.template visit<Code>();
    case Kind::heading:
        return visitor.template visit<Heading>();
    case Kind::paragraph:
        return visitor.template visit<Paragraph>();
    case Kind::link:
        return visitor.template visit<Link>();
    case Kind::reference:
        return visitor.template visit<Reference>();
    case Kind::copied:
        return visitor.template visit<Copied>();
    case Kind::list_item:
        return visitor.template visit<ListItem>();
    case Kind::param:
        return visitor.template visit<Param>();
    case Kind::returns:
        return visitor.template visit<Returns>();
    case Kind::styled:
        return visitor.template visit<Styled>();
    case Kind::text:
        return visitor.template visit<Text>();
    case Kind::tparam:
        return visitor.template visit<TParam>();
    case Kind::throws:
        return visitor.template visit<Throws>();
    default:
        MRDOCS_UNREACHABLE();
    }
}

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
    Paragraph const* brief = nullptr;
    std::vector<Block const*> blocks;
    Returns const* returns = nullptr;
    std::vector<Param const*> params;
    std::vector<TParam const*> tparams;
    std::vector<Throws const*> exceptions;
};

MRDOCS_DECL dom::String toString(Style style) noexcept;

} // doc

//------------------------------------------------

class Corpus;

/** A processed Doxygen-style comment attached to a declaration.
*/
class MRDOCS_DECL
    Javadoc
{
public:
    /** Constructor.
    */
    MRDOCS_DECL
    Javadoc() noexcept;

    /** Constructor
    */
    explicit
    Javadoc(
        doc::List<doc::Block> blocks);

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

    doc::List<doc::Block> const&
    getDescription(Corpus const& corpus) const noexcept;

    /** Return the list of top level blocks.
    */
    doc::List<doc::Block> const&
    getBlocks() const noexcept
    {
        return blocks_;
    }

    // VFALCO This is unfortunately necessary for
    //        the deserialization from bitcode...
    doc::List<doc::Block>&
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
        return emplace_back(std::make_unique<T>(std::move(block)));
    }

    /** Append blocks from another javadoc to this.
    */
    void append(Javadoc&& other);

    void append(doc::List<doc::Node>&& blocks);

private:
    std::string emplace_back(std::unique_ptr<doc::Block>);

    doc::List<doc::Block> blocks_;
};

} // mrdocs
} // clang

#endif
