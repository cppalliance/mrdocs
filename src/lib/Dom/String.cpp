//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom/String.hpp>
#include <atomic>

namespace clang {
namespace mrdox {
namespace dom {

namespace {

// Variable-length integer
template<std::unsigned_integral U>
class varint
{
    using Digit = std::uint8_t;
    static_assert(CHAR_BIT == 8);
    static_assert(sizeof(Digit) == 1);
    static_assert(std::unsigned_integral<Digit>);
    static constexpr auto N = (sizeof(U) * 8 + 6) / 7;
    static constexpr Digit Bits = 8 * sizeof(Digit) - 1;
    static constexpr Digit EndBit = 1 << Bits;
    static constexpr Digit DigMask = (1 << Bits) - 1;
    char buf_[N];
    std::size_t n_ = 0;

public:
    explicit varint(U u) noexcept
    {
        auto p = buf_;
        for(;;)
        {
            ++n_;
            auto dig = u & DigMask;
            u >>= Bits;
            if(u == 0)
            {
                // hi bit set means stop
                dig |= EndBit;
                *p = static_cast<char>(dig);
                break;
            }
            *p++ = static_cast<char>(dig);
        }
    }

    std::string_view get() const noexcept
    {
        return { buf_, n_ };
    }

    static U read(char const*& p) noexcept
    {
        Digit dig = *p++;
        if(dig & EndBit)
            return dig & DigMask;
        U u = dig;
        unsigned Shift = Bits;
        for(;;)
        {
            auto dig = *p++;
            if(dig & EndBit)
            {
                dig &= DigMask;
                u |= dig << Shift;
                return u;
            }
            u |= dig << Shift;
            Shift += Bits;
        }
    }
};

static constinit char sz_empty = { '\0' };

} // (anon)

// An allocated string is stored in one buffer
// laid out thusly:
//
// Impl
// size     varint
// chars    char[]
//
struct String::Impl
{
    std::atomic<std::size_t> refs;

    explicit Impl(
        std::string_view s,
        varint<std::size_t> const& uv) noexcept
        : refs(1)
    {
        auto p = reinterpret_cast<char*>(this+1);
        auto vs = uv.get();
        std::memcpy(p, vs.data(), vs.size());
        p += vs.size();
        std::memcpy(p, s.data(), s.size());
        p[s.size()] = '\0';
    }

    std::string_view get() const noexcept
    {
        auto p = reinterpret_cast<char const*>(this+1);
        auto const size = varint<std::size_t>::read(p);
        return { p, size };
    }
};

void
String::
allocate(
    std::string_view s,
    Impl*& impl,
    char const*& psz)
{
    std::allocator<Impl> alloc;
    varint<std::size_t> uv(s.size());
    auto const varlen = uv.get().size();
    auto n =
        sizeof(Impl) +      // header
        varlen +            // size (varint)
        s.size() +          // string data
        1 +                 // null term '\0'
        (sizeof(Impl) - 1); // round up to nearest sizeof(Impl)
    impl = new(alloc.allocate(n / sizeof(Impl))) Impl(s, uv);
    psz = reinterpret_cast<char const*>(impl + 1) + varlen;
}

void
String::
deallocate(
    Impl* impl) noexcept
{
    std::allocator<Impl> alloc;
    auto const s = impl->get();
    varint<std::size_t> uv(s.size());
    auto const varlen = uv.get().size();
    auto n =
        sizeof(Impl) +      // header
        varlen +            // size (varint)
        s.size() +          // string data
        1 +                 // null term '\0'
        (sizeof(Impl) - 1); // round up to nearest sizeof(Impl)
    std::destroy_at(impl);
    alloc.deallocate(impl, n / sizeof(Impl));
}

//------------------------------------------------

String::
~String()
{
    if(is_literal())
        return;
    if(--impl_->refs > 0)
        return;
    deallocate(impl_);
}

String::
String() noexcept
    : len_(len(0))
    , psz_(&sz_empty)
{
}

String::
String(
    String&& other) noexcept
    : String()
{
    swap(other);
}

String::
String(
    String const& other) noexcept
    : impl_(other.impl_)
    , psz_(other.psz_)
{
    if(! is_literal())
        ++impl_->refs;
}

String::
String(
    std::string_view s)
{
    allocate(s, impl_, psz_);
    MRDOX_ASSERT(! is_literal());
}

String&
String::
operator=(
    String&& other) noexcept
{
    String temp(std::move(other));
    swap(temp);
    return *this;
}

String&
String::
operator=(
    String const& other) noexcept
{
    String temp(other);
    swap(temp);
    return *this;
}

std::string_view
String::
get() const noexcept
{
    if(is_literal())
        return std::string_view(psz_,
            (len_ & ((std::size_t(-1) >> 1) & ~std::size_t(1))) |
            (len_ >> (sizeof(std::size_t)*8 - 1)));
    return impl_->get();
}

} // dom
} // mrdox
} // clang
