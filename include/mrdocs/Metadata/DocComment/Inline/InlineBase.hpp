//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_METADATA_DOCCOMMENT_INLINE_INLINEBASE_HPP
#define MRDOCS_API_METADATA_DOCCOMMENT_INLINE_INLINEBASE_HPP

#include <mrdocs/Platform.hpp>
#include <mrdocs/Dom/LazyArray.hpp>
#include <mrdocs/Dom/LazyObject.hpp>
#include <mrdocs/Metadata/DocComment/Inline/InlineKind.hpp>
#include <mrdocs/Metadata/DomCorpus.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>
#include <string>

namespace mrdocs::doc {

/* Forward declarations
 */
#define INFO(Type) struct Type##Inline;
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

/** A Node containing a string of text.

    There will be no newlines in the text. Otherwise,
    this would be represented as multiple text nodes
    within a Paragraph node.
*/
struct MRDOCS_DECL Inline
{
    /** Discriminator identifying which inline variant is active.
    */
    InlineKind Kind = InlineKind::Text;

    /** Virtual destructor to enable polymorphic deletion.
    */
    virtual ~Inline() = default;

    /** Three-way comparison by active inline kind and data.
    */
    auto operator<=>(Inline const&) const = default;
    /** Equality compares active kind and stored data.
    */
    bool operator==(Inline const&) const noexcept = default;

    /** View as const Inline reference.
    */
    constexpr Inline const& asInline() const noexcept
    {
        return *this;
    }

    /** View as mutable Inline reference.
    */
    constexpr Inline& asInline() noexcept
    {
        return *this;
    }

    #define INFO(Type) constexpr bool is##Type() const noexcept { \
        return Kind == InlineKind::Type; \
    }
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

#define INFO(Type) \
    constexpr Type##Inline const& as##Type() const noexcept { \
        if (Kind == InlineKind::Type) \
            return reinterpret_cast<Type##Inline const&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

#define INFO(Type) \
    constexpr Type##Inline & as##Type() noexcept { \
        if (Kind == InlineKind::Type) \
            return reinterpret_cast<Type##Inline&>(*this); \
        MRDOCS_UNREACHABLE(); \
    }
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

#define INFO(Type)                                               \
    constexpr Type##Inline const* as##Type##Ptr() const noexcept \
    {                                                            \
        if (Kind == InlineKind::Type)                            \
        {                                                        \
            return reinterpret_cast<Type##Inline const*>(this);  \
        }                                                        \
        return nullptr;                                          \
    }
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

#define INFO(Type) \
    constexpr Type##Inline * as##Type##Ptr() noexcept { \
        if (Kind == InlineKind::Type) { return reinterpret_cast<Type##Inline *>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

protected:
    /** Default-construct a text inline.
    */
    constexpr Inline() noexcept = default;

    /** Construct with a specific inline kind.
    */
    Inline(
        InlineKind kind_)
        : Kind(kind_)
    {
    }
};

/** Base class for providing variant discriminator functions.

    This offers functions that return a boolean at
    compile-time, indicating if the most-derived
    class is a certain type.
*/
template <InlineKind K>
struct InlineCommonBase : Inline
{
    /** The variant discriminator constant of the most-derived class.

        It only distinguishes from `Inline::kind` in that it is a constant.

    */
    static constexpr InlineKind kind_id = K;

    /** Virtual destructor to preserve polymorphic cleanup.
    */
    virtual ~InlineCommonBase() override = default;

    #define INFO(Kind) \
    static constexpr bool is##Kind() noexcept { return K == InlineKind::Kind; }
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

    /** Three-way comparison forwards to the underlying inline.
    */
    auto operator<=>(InlineCommonBase const&) const = default;

    MRDOCS_DESCRIBE_CLASS(InlineCommonBase, (Inline), ())

protected:
    /** Default-construct with the fixed inline kind.
    */
    constexpr explicit InlineCommonBase()
        : Inline(K)
    {}
};


MRDOCS_DESCRIBE_STRUCT(
    Inline,
    (),
    (Kind)
)

/** Get the plain text representation of an inline element tree.

    This strips all formatting and returns just the text content.

    @param in The input inline element.
    @param dst The output string to append to.
*/
MRDOCS_DECL
void
getAsPlainText(doc::Inline const& in, std::string& dst);

/** Get the plain text representation of an inline element tree.

    This strips all formatting and returns just the text content.

    @param in The input inline element.
    @return The flattened plain text.
*/
inline
std::string
getAsPlainText(doc::Inline const& in)
{
    std::string dst;
    getAsPlainText(in, dst);
    return dst;
}

/// An internal node in the inline element tree.
struct MRDOCS_DECL InlineContainer
{
    /** Child inline elements contained here.
    */
    std::vector<Polymorphic<Inline>> children;

    /** Virtual destructor for derived containers.
    */
    virtual ~InlineContainer() = default;

    /** Default-construct an empty container.
    */
    InlineContainer() = default;

    /** Copy-construct all child nodes.
    */
    InlineContainer(InlineContainer const&) = default;

    /** Move-construct child nodes.
    */
    InlineContainer(InlineContainer&&) noexcept = default;

    /// Construct an InlineContainer with a single TextInline child.
    explicit
    InlineContainer(std::string_view text);

    /// Construct an InlineContainer with a single TextInline child.
    explicit
    InlineContainer(char const* text)
        : InlineContainer(std::string_view(text))
    {}

    /// Construct an InlineContainer with a single TextInline child.
    explicit
    InlineContainer(std::string const& text);

    /// Construct an InlineContainer with a single TextInline child.
    explicit
    InlineContainer(std::string&& text);

    /** Copy-assign child nodes.
    */
    InlineContainer&
    operator=(InlineContainer const&) = default;

    /** Move-assign child nodes.
    */
    InlineContainer&
    operator=(InlineContainer&&) noexcept = default;

    /// Assign an InlineContainer with a single TextInline child.
    InlineContainer&
    operator=(std::string_view text);

    /** Return this container as a base-class reference.

        @return A reference to the underlying InlineContainer.
    */
    InlineContainer&
    asInlineContainer()
    {
        return *this;
    }

    /// @copydoc asInlineContainer()
    InlineContainer const&
    asInlineContainer() const
    {
        return *this;
    }

    /// Get the first inline child.
    Polymorphic<Inline> const&
    front() const
    {
        MRDOCS_ASSERT(!children.empty());
        return children.front();
    }

    /// Get the first inline child.
    Polymorphic<Inline>&
    front()
    {
        MRDOCS_ASSERT(!children.empty());
        return children.front();
    }

    /// Get the last inline child.
    Polymorphic<Inline> const&
    back() const
    {
        MRDOCS_ASSERT(!children.empty());
        return children.back();
    }

    /// Get the last inline child.
    Polymorphic<Inline>&
    back()
    {
        MRDOCS_ASSERT(!children.empty());
        return children.back();
    }

    /// Determine if there are no inline children.
    bool
    empty() const noexcept
    {
        return children.empty();
    }

    /// Get the number of inline children.
    std::size_t
    size() const noexcept
    {
        return children.size();
    }

    /** Begin iterator forwarding to children.

        @param self The container instance.
        @return Iterator to the first child.
    */
    decltype(auto)
    begin(this auto&& self) noexcept
    {
        return self.children.begin();
    }

    /** End iterator forwarding to children.

        @param self The container instance.
        @return Iterator past the last child.
    */
    decltype(auto)
    end(this auto&& self) noexcept
    {
        return self.children.end();
    }

    /** Erase inline children.

        @param self The container instance.
        @param args Arguments forwarded to `std::vector::erase`.
        @return The iterator returned by `erase`.
    */
    decltype(auto)
    erase(this auto&& self, auto&&... args)
    {
        return self.children.erase(std::forward<decltype(args)>(args)...);
    }

    /** Insert inline children.

        @param self The container instance.
        @param args Arguments forwarded to `std::vector::insert`.
        @return The iterator returned by `insert`.
    */
    decltype(auto)
    insert(this auto&& self, auto&&... args)
    {
        return self.children.insert(std::forward<decltype(args)>(args)...);
    }

    /// Clear all inline children.
    void
    clear()
    {
        children.clear();
    }

    /** Append a TextInline child.

        @param text The text to append.
        @return A reference to this container.
    */
    InlineContainer&
    append(std::string_view text);

    /** Append a child of the specified type.

        @tparam InlineTy The Inline-derived type to add.
        @param args Constructor arguments forwarded to the child.
        @return A reference to this container.
    */
    template <std::derived_from<Inline> InlineTy, class... Args>
    InlineContainer&
    append(Args&&... args)
    {
        children.push_back(Polymorphic<Inline>(std::in_place_type<InlineTy>, std::forward<Args>(args)...));
        return *this;
    }

    /// Append a TextInline child.
    InlineContainer&
    operator+=(std::string_view text)
    {
        return append(text);
    }

    /// Append an inline child.
    template <std::derived_from<Inline> InlineTy>
    InlineContainer&
    operator+=(InlineTy&& inlineChild)
    {
        children.push_back(Polymorphic<Inline>(std::in_place_type<InlineTy>, std::forward<InlineTy>(inlineChild)));
        return *this;
    }

    /** Append a child of the specified type in-place.

        @tparam InlineTy The Inline-derived type to emplace.
        @param args Constructor arguments forwarded to the child.
        @return A reference to this container.
    */
    template <std::derived_from<Inline> InlineTy, class... Args>
    InlineContainer&
    emplace_back(Args&&... args)
    {
        children.push_back(Polymorphic<Inline>(std::in_place_type<InlineTy>, std::forward<Args>(args)...));
        return *this;
    }

    /** Compare two InlineContainers.
    */
    std::strong_ordering
    operator<=>(InlineContainer const&) const;

    /** Equality compares child sequences.
    */
    bool
    operator==(InlineContainer const&) const = default;
};

MRDOCS_DESCRIBE_STRUCT(
    InlineContainer,
    (),
    (children)
)

/** Serialize a polymorphic inline node into a DOM value.
    @param io Destination value.
    @param I Inline storage to convert.
    @param domCorpus Corpus context for lazy references.
*/
template <class IO, polymorphic_storage_for<Inline> InlineTy>
void
tag_invoke(
    dom::ValueFromTag,
    IO& io,
    InlineTy const& I,
    DomCorpus const* domCorpus);

/** Removes leading whitespace from the first text element in the given InlineContainer.

    @param inlines The InlineContainer to trim.
    @return void
*/
MRDOCS_DECL
void
ltrim(InlineContainer& inlines);

/** Removes trailing whitespace from the last text element in the given InlineContainer.

    @param inlines The InlineContainer to trim.
    @return void
*/
MRDOCS_DECL
void
rtrim(InlineContainer& inlines);

/** Removes leading and trailing whitespace from the text elements in the given InlineContainer.

    @param inlines The InlineContainer to trim.
    @return void
*/
inline
void
trim(InlineContainer& inlines)
{
    ltrim(inlines);
    rtrim(inlines);
}

/** Flatten an InlineContainer to plain text.

    This concatenates all text nodes, ignoring formatting.

    @param in The InlineContainer to flatten.
    @param dst The output string to append to.
*/
MRDOCS_DECL
void
getAsPlainText(doc::InlineContainer const& in, std::string& dst);

/** Flatten an InlineContainer to plain text.

    This concatenates all text nodes, ignoring formatting.

    @param in The InlineContainer to flatten.
    @return The flattened plain text.
*/
inline
std::string
getAsPlainText(doc::InlineContainer const& in)
{
    std::string dst;
    getAsPlainText(in, dst);
    return dst;
}

/// A leaf node that stores a string of text.
struct MRDOCS_DECL InlineTextLeaf
{
    /** Raw literal text stored in this leaf node.
    */
    std::string literal;

    /** Construct from a string view.
    */
    explicit
    InlineTextLeaf(std::string_view literal_)
        : literal(literal_)
    {}

    /** Construct from a string copy.
    */
    explicit
    InlineTextLeaf(std::string const& literal_)
        : literal(literal_)
    {}

    /** Construct by moving a string.
    */
    explicit
    InlineTextLeaf(std::string&& literal_) noexcept
        : literal(std::move(literal_))
    {}

    /** Order text leaves lexicographically by content.
    */
    auto operator<=>(InlineTextLeaf const&) const = default;
    /** Equality compares literal text.
    */
    bool operator==(InlineTextLeaf const&) const noexcept = default;
};


MRDOCS_DESCRIBE_STRUCT(
    InlineTextLeaf,
    (),
    (literal)
)

} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_INLINEBASE_HPP
