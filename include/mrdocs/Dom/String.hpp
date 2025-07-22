//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_DOM_STRING_HPP
#define MRDOCS_API_DOM_STRING_HPP

#include <format>
#include <mrdocs/Platform.hpp>
#include <mrdocs/Support/String.hpp>

namespace clang {
namespace mrdocs {
namespace dom {

class String;

/** Satisfied if StringTy is convertible to String but not a String.
*/
template<class StringTy>
concept StringLikeTy =
    ! std::is_same_v<StringTy, String> &&
    std::convertible_to<StringTy, std::string_view>;

class MRDOCS_DECL
    String final
{
    char const* ptr_ = nullptr;

    bool
    is_literal() const noexcept
    {
        MRDOCS_ASSERT(! empty());
        // for string literals, data_ stores a pointer
        // to the first character of the string.
        // for ref-counted strings, data_ stores a pointer
        // to the null-terminator of the string.
        return *ptr_;
    }

    class impl_view;

    impl_view impl() const noexcept;

    /** Construct a ref-counted string.
    */
    void
    construct(
        char const* s,
        std::size_t n);

public:
    /** Constructor.

        Default constructed strings have a zero size,
        and include a null terminator.
    */
    constexpr String() noexcept = default;

    /** Constructor.

        Ownership of the string is transferred to
        the newly constructed string. The moved-from
        string behaves as if default constructed.
    */
    constexpr String(String&& other) noexcept
    {
        swap(other);
    }

    /** Constructor.

        The newly constructed string acquries shared
        ownership of the string referenced by other.
    */
    String(String const& other) noexcept;

    /** Destructor.
    */
    ~String() noexcept;

    /** Constructor.

        This function constructs a string literal
        which references the buffer pointed to by
        `str`. Ownership is not transferred; the lifetime
        of the buffer must extend until the string is
        destroyed, otherwise the behavior is undefined.

        @param str A null-terminated string. If the
        string is not null-terminated, the result is
        undefined.
    */
    template<std::size_t N>
    constexpr
    String(const char(&str)[N])
    {
        // empty strings are stored as nullptr
        if constexpr(N > 1)
            ptr_ = str;
    }

    /** Constructor.

        This function constructs a new string from
        the string pointed to by `str` of length `len`.

        @param `str` The string to construct with.
        A copy of this string is made.
        @param `len` The length of the string.
    */
    String(
        char const* str,
        std::size_t len)
    {
        // empty strings are stored as nullptr
        if(len)
            construct(str, len);
    }

    /** Constructor.

        This function constructs a new string from
        the buffer pointed to by `sv`.

        @param sv The string to construct with.
        A copy of this string is made.
    */
    String(std::string_view sv)
        : String(sv.data(), sv.size())
    {
    }

    /** Constructor.

        This function constructs a new string from
        s, which must be convertible to `std::string_view`.

        @param s The string to construct with.
        A copy of this string is made.
    */
    template<StringLikeTy StringLike>
    String(StringLike const& s)
        : String(std::string_view(s))
    {
    }

    /** Assignment.

        This acquires shared ownership of the
        string referenced by other. Ownership of
        the previously referenced string is released.
    */
    String&
    operator=(
        String const& other) noexcept
    {
        String temp(other);
        swap(temp);
        return *this;
    }

    /** Assignment.

        This transfers ownership of the string
        referenced by other to this. Ownership of
        the previously referened string is released.
        After the assignment, the moved-from string
        behaves as if default constructed.
    */
    String&
    operator=(
        String&& other) noexcept
    {
        String temp(std::move(other));
        swap(temp);
        return *this;
    }

    /** Return the string.
    */
    operator std::string_view() const noexcept
    {
        return get();
    }

    /** Return the string.
    */
    std::string_view
    get() const noexcept
    {
        return std::string_view(
            data(), size());
    }

    /** Return the string.
    */
    std::string str() const noexcept
    {
        return std::string(get());
    }

    /** Return true if the string is empty.
    */
    bool
    empty() const noexcept
    {
        return ! ptr_;
    }

    /** Return the size.
    */
    std::size_t size() const noexcept;

    /** Return the string.

        The pointed-to character buffer returned
        by this function is always null-terminated.
    */
    char const* data() const noexcept;

    /** Return the string.

        The pointed-to character buffer returned
        by this function is always null-terminated.
    */
    char const* c_str() const noexcept
    {
        return data();
    }

    /** Swap two strings.
    */
    constexpr
    void
    swap(String& other) noexcept
    {
        std::swap(ptr_, other.ptr_);
    }

    /** Swap two strings.
    */
    friend
    constexpr
    void
    swap(
        String& lhs,
        String& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    /** Return the result of comparing two strings.
    */
    template<StringLikeTy StringLike>
    friend bool operator==(String const& lhs, StringLike const& rhs) noexcept
    {
        return lhs.get() == std::string_view(rhs);
    }

    /** Return the result of comparing two strings.
    */
    template<StringLikeTy StringLike>
    friend bool operator!=(String const& lhs, StringLike const& rhs) noexcept
    {
        return lhs.get() != std::string_view(rhs);
    }

    /** Return the result of comparing two strings.
    */
    template<StringLikeTy StringLike>
    friend auto operator<=>(String const& lhs, StringLike const& rhs) noexcept
    {
        return lhs.get() <=> std::string_view(rhs);
    }

    /** Return the result of comparing two strings.
    */
    friend bool operator==(
        String const& lhs, String const& rhs) noexcept
    {
        return lhs.get() == rhs.get();
    }

    /** Return the result of comparing two strings.
    */
    friend bool operator!=(
        String const& lhs, String const& rhs) noexcept
    {
        return lhs.get() != rhs.get();
    }

    /** Return the result of comparing two strings.
    */
    friend auto operator<=>(
        String const& lhs, String const& rhs) noexcept
    {
        return lhs.get() <=> rhs.get();
    }

    /** Concatenate two strings.
    */
    friend auto operator+(
        String const& lhs, String const& rhs) noexcept
    {
        std::string buf(lhs.get());
        buf.append(rhs.get());
        return String(buf);
    }

    /// @overload
    template <StringLikeTy S>
    friend auto operator+(
        S const& lhs, String const& rhs) noexcept
    {
        std::string buf(lhs);
        buf.append(rhs.get());
        return String(buf);
    }

    /// @overload
    template <StringLikeTy S>
    friend auto operator+(
        String const& lhs, S const& rhs) noexcept
    {
        std::string buf(lhs);
        buf.append(rhs);
        return String(buf);
    }
};

} // dom
} // mrdocs
} // clang

//------------------------------------------------

template <>
struct std::formatter<clang::mrdocs::dom::String>
    : std::formatter<std::string_view> {
  template <class FmtContext>
  auto format(clang::mrdocs::dom::String const &value, FmtContext &ctx) const {
    return std::formatter<std::string_view>::format(value.get(), ctx);
  }
};

#endif
