//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/String.hpp>
#include <atomic>
#include <cstring>


namespace mrdocs {
namespace dom {

class String::impl_view
{
    char* impl_;

public:
    constexpr
    impl_view(char const* ptr)
        : impl_(const_cast<char*>(ptr))
    {
    }

    std::size_t size()
    {
        std::size_t n;
        std::memcpy(&n, impl_ + 1, sizeof(std::size_t));
        return n;
    }

    char* data()
    {
        return impl_ - size();
    }

    char* base()
    {
        return data() - sizeof(std::atomic<std::size_t>);
    }

    std::atomic<std::size_t>& refs()
    {
        return *reinterpret_cast<
            std::atomic<std::size_t>*>(base());
    }
};

String::impl_view
String::
impl() const noexcept
{
    MRDOCS_ASSERT(! empty() && ! is_literal());
    return impl_view(ptr_);
}

void
String::
construct(
    char const* s,
    std::size_t n)
{
    char* ptr = static_cast<char*>(::operator new(
        sizeof(std::atomic<std::size_t>) + // ref count
        n + // string
        1 + // null terminator
        sizeof(std::size_t) // string length (unaligned)
    ));
    // initialize ref count
    ::new(ptr) std::atomic<std::size_t>(1);
    ptr += sizeof(std::atomic<std::size_t>);
    // copy in the string
    std::memcpy(ptr, s, n);
    ptr += n;
    // write the null terminator
    *ptr = '\0';
    // store the address of the null terminator into ptr_.
    // it indicates that the string is ref-counted
    ptr_ = ptr++;
    // KRYSTIAN NOTE: the size is currently stored unaligned.
    // in the future, we should investigate whether the cost
    // of the cacheline split load is worth allocating
    // additional bytes to properly align this.
    // write the strings size
    std::memcpy(ptr, &n, sizeof(std::size_t));
}

String::
String(String const& other) noexcept
    : ptr_(other.ptr_)
{
    if(! empty() && ! is_literal())
        ++impl().refs();
}

String::
~String() noexcept
{
    // this better be true, since we don't call
    // any destructors when deallocating
    static_assert(
        std::is_trivially_destructible_v<
            std::atomic<std::size_t>>);
    if(empty() || is_literal())
        return;
    if(! --impl().refs())
        ::operator delete(impl().base());
}

std::size_t
String::
size() const noexcept
{
    if(empty())
        return 0;
    if(is_literal())
        return std::strlen(ptr_);
    return impl().size();
}

char const*
String::
data() const noexcept
{
    // if the string is empty, the storage
    // typically for the pointer is used
    // as a empty null terminated string
    if(empty())
        return reinterpret_cast<char const*>(&ptr_);
    if(is_literal())
        return ptr_;
    return impl().data();
}

} // dom
} // mrdocs

