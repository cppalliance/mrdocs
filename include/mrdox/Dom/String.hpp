//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_DOM_STRING_HPP
#define MRDOX_API_DOM_STRING_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/String.hpp>
#include <fmt/format.h>

namespace clang {
namespace mrdox {
namespace dom {

class String;

/** Satisfied if StringTy is convertible to String but not a String.
*/
template<class StringTy>
concept StringLikeTy =
    ! std::is_same_v<StringTy, String> &&
    std::convertible_to<StringTy, std::string_view>;

/** An immutable string with shared ownership.
*/
class MRDOX_DECL
    String final
{
    struct Impl;

    union
    {
        Impl* impl_;
        // len is stored with the low bit moved to
        // the hi bit, and the low bit always set.
        std::size_t len_;
    };
    char const* psz_;

    static void allocate(std::string_view s,Impl*&,  char const*&);
    static void deallocate(Impl*) noexcept;
    static consteval std::size_t len(std::size_t n)
    {
        return (n << (sizeof(std::size_t)*8 - 1)) | n | 1UL;
    }

    constexpr bool is_literal() const noexcept
    {
        return (len_ & 1) != 0;
    }

public:
    /** Destructor.
    */
    ~String();

    /** Constructor.

        Default constructed strings have a zero size,
        and include a null terminator.
    */
    String() noexcept;

    /** Constructor.

        Ownership of the string is transferred to
        the newly constructed string. The moved-from
        string behaves as if default constructed.
    */
    String(String&& other) noexcept;

    /** Constructor.

        The newly constructed string acquries shared
        ownership of the string referenced by other.
    */
    String(String const& other) noexcept;

    /** Constructor.

        This function constructs a new string from
        the buffer pointed to by `s`.

        @param s The string to construct with.
        A copy of this string is made.
    */
    String(std::string_view s);

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

    /** Constructor.

        This function constructs a string literal
        which references the buffer pointed to by
        sz. Ownership is not transferred; the lifetime
        of the buffer must extend until the string is
        destroyed, otherwise the behavior is undefined.

        @param psz A null-terminated string. If the
        string is not null-terminated, the result is
        undefined.
    */
    template<std::size_t N>
    constexpr String(char const(&psz)[N]) noexcept
        : len_(len(N-1))
        , psz_(psz)
    {
        static_assert(N > 0);
        static_assert(N <= std::size_t(-1)>>1);
    }

    /** Assignment.

        This transfers ownership of the string
        referenced by other to this. Ownership of
        the previously referened string is released.
        After the assignment, the moved-from string
        behaves as if default constructed.
    */
    String& operator=(String&& other) noexcept;

    /** Assignment.

        This acquires shared ownership of the
        string referenced by other. Ownership of
        the previously referenced string is released.
    */
    String& operator=(String const& other) noexcept;

    /** Return true if the string is empty.
    */
    constexpr bool empty() const noexcept
    {
        return psz_[0] == '\0';
    }

    /** Return the string.
    */
    std::string_view
    get() const noexcept;

    /** Return the string.
    */
    operator std::string_view() const noexcept
    {
        return get();
    }

    /** Return the string.
    */
    std::string str() const noexcept
    {
        return std::string(get());
    }

    /** Return the size.
    */
    std::size_t size() const noexcept
    {
        return get().size();
    }

    /** Return the size.
    */
    char const* data() const noexcept
    {
        return get().data();
    }

    /** Return the string.

        The pointed-to character buffer returned
        by this function is always null-terminated.
    */
    char const* c_str() const noexcept
    {
        return psz_;
    }

    /** Swap two strings.
    */
    void swap(String& other) noexcept
    {
        std::swap(impl_, other.impl_);
        std::swap(psz_, other.psz_);
    }

    /** Swap two strings.
    */
    friend void swap(String& lhs, String& rhs) noexcept
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
};

} // dom
} // mrdox
} // clang

//------------------------------------------------

template<>
struct fmt::formatter<clang::mrdox::dom::String>
    : fmt::formatter<std::string_view>
{
    auto format(
        clang::mrdox::dom::String const& value,
        fmt::format_context& ctx) const
    {
        return fmt::formatter<std::string_view>::format(
            value.get(), ctx);
    }
};

#endif
