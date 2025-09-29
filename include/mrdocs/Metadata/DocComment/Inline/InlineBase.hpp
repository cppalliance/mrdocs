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
    InlineKind Kind = InlineKind::Text;

    virtual ~Inline() = default;

    auto operator<=>(Inline const&) const = default;
    bool operator==(Inline const&) const noexcept = default;

    constexpr Inline const& asInline() const noexcept
    {
        return *this;
    }

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

#define INFO(Type) \
    constexpr Type##Inline const* as##Type##Ptr() const noexcept { \
        if (Kind == InlineKind::Type) { return reinterpret_cast<Type##Inline const*>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

#define INFO(Type) \
    constexpr Type##Inline * as##Type##Ptr() noexcept { \
        if (Kind == InlineKind::Type) { return reinterpret_cast<Type##Inline *>(this); } \
        return nullptr; \
    }
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

protected:
    constexpr Inline() noexcept = default;

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

    virtual ~InlineCommonBase() override = default;

    #define INFO(Kind) \
    static constexpr bool is##Kind() noexcept { return K == InlineKind::Kind; }
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

    auto operator<=>(InlineCommonBase const&) const = default;

protected:
    constexpr explicit InlineCommonBase()
        : Inline(K)
    {}
};


/** Map the @ref Inline to a @ref dom::Object.

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
    Inline const& I,
    DomCorpus const* domCorpus)
{
    io.map("kind", toString(I.Kind));
}

/** Return the @ref Inline as a @ref dom::Value object.
 */
inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    Inline const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

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

/// An internal node in the inline element tree
struct MRDOCS_DECL InlineContainer
{
    std::vector<Polymorphic<Inline>> children;

    virtual ~InlineContainer() = default;

    InlineContainer() = default;

    InlineContainer(InlineContainer const&) = default;

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

    InlineContainer&
    operator=(InlineContainer const&) = default;

    InlineContainer&
    operator=(InlineContainer&&) noexcept = default;

    /// Assign an InlineContainer with a single TextInline child.
    InlineContainer&
    operator=(std::string_view text);

    /// Helper function so that derived classes can get a reference to
    /// the base class.
    InlineContainer&
    asInlineContainer()
    {
        return *this;
    }

    /// @copydoc asInlineContainer
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

    /// Begin iterator forwarding to children
    decltype(auto)
    begin(this auto&& self) noexcept
    {
        return self.children.begin();
    }

    /// End iterator forwarding to children
    decltype(auto)
    end(this auto&& self) noexcept
    {
        return self.children.end();
    }

    /// Erase from children
    decltype(auto)
    erase(this auto&& self, auto&&... args)
    {
        return self.children.erase(std::forward<decltype(args)>(args)...);
    }

    /// Erase from children
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

    /// Append a TextInline child.
    InlineContainer&
    append(std::string_view text);

    /// Append a child of the specified type.
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

    /// Append a child of the specified type.
    template <std::derived_from<Inline> InlineTy, class... Args>
    InlineContainer&
    emplace_back(Args&&... args)
    {
        children.push_back(Polymorphic<Inline>(std::in_place_type<InlineTy>, std::forward<Args>(args)...));
        return *this;
    }

    /// Compare two InlineContainers.
    std::strong_ordering
    operator<=>(InlineContainer const&) const;

    /// Compare two InlineContainers.
    bool
    operator==(InlineContainer const&) const = default;
};

template <class IO, polymorphic_storage_for<Inline> InlineTy>
void
tag_invoke(
    dom::ValueFromTag,
    IO& io,
    InlineTy const& I,
    DomCorpus const* domCorpus);

template <class IO>
void
tag_invoke(
    dom::LazyObjectMapTag t,
    IO& io,
    InlineContainer const& I,
    DomCorpus const* domCorpus)
{
    io.defer("children", [&I, domCorpus] {
        return dom::LazyArray(I.children, domCorpus);
    });
}

inline
void
tag_invoke(
    dom::ValueFromTag,
    dom::Value& v,
    InlineContainer const& I,
    DomCorpus const* domCorpus)
{
    v = dom::LazyObject(I, domCorpus);
}

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
    std::string literal;

    explicit
    InlineTextLeaf(std::string_view literal_)
        : literal(literal_)
    {}

    explicit
    InlineTextLeaf(std::string const& literal_)
        : literal(literal_)
    {}

    explicit
    InlineTextLeaf(std::string&& literal_) noexcept
        : literal(std::move(literal_))
    {}

    auto operator<=>(InlineTextLeaf const&) const = default;
    bool operator==(InlineTextLeaf const&) const noexcept = default;
};


} // mrdocs::doc

#endif // MRDOCS_API_METADATA_DOCCOMMENT_INLINE_INLINEBASE_HPP
