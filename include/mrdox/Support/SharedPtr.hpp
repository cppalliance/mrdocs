//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_SUPPORT_SHAREDPTR_HPP
#define MRDOX_API_SUPPORT_SHAREDPTR_HPP

#include <mrdox/Platform.hpp>
#include <atomic>

namespace clang {
namespace mrdox {

template<class T> class SharedPtr;
template<class T> class AtomicSharedPtr;

template<class U, class... Args>
SharedPtr<U> makeShared(Args&&...);

//------------------------------------------------

/** The native object type used in SharedPtr.
*/
class SharedBase
{
private:
    template<class T> friend class SharedPtr;
    template<class T> friend class AtomicSharedPtr;

    template<class U, class... Args>
    friend SharedPtr<U> makeShared(Args&&...);

    virtual ~SharedBase() = default;
    SharedBase() noexcept = default;
    SharedBase* addref() noexcept;
    void release() noexcept;
    virtual void* get() noexcept = 0;
    std::atomic<std::size_t> refs = 0;
};

inline
auto
SharedBase::
SharedBase::
addref() noexcept ->
    SharedBase*
{
    ++refs;
    return this;
}

inline
void
SharedBase::
SharedBase::
release() noexcept
{
    if(--refs > 0)
        return;
    delete this;
}

//------------------------------------------------

/** A simple smart pointer container with shared ownership.
*/
template<class T>
class SharedPtr
{
    SharedBase* p_ = nullptr;

    template<class U> friend class SharedPtr;
    template<class U> friend class AtomicSharedPtr;

    explicit SharedPtr(SharedBase*) noexcept;
    explicit SharedPtr(SharedBase*, int) noexcept;

public:
    ~SharedPtr();
    SharedPtr() noexcept = default;
    SharedPtr(SharedPtr const& other) noexcept;
    SharedPtr& operator=(SharedPtr const& other) noexcept;

    template<class U>
    requires std::convertible_to<U*, T*>
    SharedPtr(SharedPtr<U> const& other) noexcept;

    template<class U>
    requires std::convertible_to<U*, T*>
    SharedPtr& operator=(SharedPtr<U> const& other) noexcept;

    T* get() const noexcept;
    T* operator->() const noexcept;
    T& operator*() const noexcept;
    explicit operator bool() const noexcept;

    template<class U, class... Args>
    friend SharedPtr<U> makeShared(Args&&...);

    // unsafe access

    SharedBase* releaseUnsafe() noexcept;

    template<class U>
    friend SharedPtr<U>
    acquireUnsafe(SharedBase* p) noexcept;

    // swap

    void swap(SharedPtr& rhs) noexcept;

    friend void swap(
        SharedPtr& lhs, SharedPtr& rhs) noexcept
    {
        lhs.swap(rhs);
    }
};

//------------------------------------------------

template<class T>
SharedPtr<T>::
SharedPtr(
    SharedBase* p) noexcept
    : p_(p ? p->addref() : nullptr)
{
}

template<class T>
SharedPtr<T>::
SharedPtr(
    SharedBase* p, int) noexcept
    : p_(p)
{
}

template<class T>
SharedPtr<T>::
~SharedPtr()
{
    if(p_)
        p_->release();
}

template<class T>
SharedPtr<T>::
SharedPtr(
    SharedPtr const& other) noexcept
    : p_(other.p_ ? other.p_->addref() : nullptr)
{
}

template<class T>
auto
SharedPtr<T>::
operator=(
    SharedPtr const& other) noexcept ->
    SharedPtr&
{
    if(other.p_)
        other.p_->addref();
    if(p_)
        p_->release();
    p_ = other.p_;
    return *this;
}

template<class T>
template<class U>
requires std::convertible_to<U*, T*>
SharedPtr<T>::
SharedPtr(
    SharedPtr<U> const& other) noexcept
    : p_(other.p_ ? other.p_->addref() : nullptr)
{
}

template<class T>
template<class U>
requires std::convertible_to<U*, T*>
auto
SharedPtr<T>::
operator=(
    SharedPtr<U> const& other) noexcept ->
    SharedPtr&
{
    if(other.p_)
        other.p_->addref();
    if(p_)
        p_->release();
    p_ = other.p_;
    return *this;
}

template<class T>
T*
SharedPtr<T>::
get() const noexcept
{
    return static_cast<T*>(p_->get());
}

template<class T>
T*
SharedPtr<T>::
operator->() const noexcept
{
    return get();
}

template<class T>
T&
SharedPtr<T>::
operator*() const noexcept
{
    return *get();
}

template<class T>
SharedPtr<T>::
operator bool() const noexcept
{
    return p_ != nullptr;
}

template<class T>
SharedBase*
SharedPtr<T>::
releaseUnsafe() noexcept
{
    auto p = p_;
    p_ = nullptr;
    return p;
}

template<class U>
SharedPtr<U>
acquireUnsafe(
    SharedBase* p) noexcept
{
    return SharedPtr<U>(p, 0);
}

template<class T>
void
SharedPtr<T>::
swap(
    SharedPtr& rhs) noexcept
{
    std::swap(p_, rhs.p_);
}

//------------------------------------------------

template<class U, class... Args>
SharedPtr<U>
makeShared(Args&&... args)
{
    struct Impl : SharedBase
    {
        U u;

        explicit Impl(Args&&... args)
            : u(std::forward<Args>(args)...)
        {
        }

        void* get() noexcept override
        {
            return &u;
        }
    };

    return SharedPtr<U>(new Impl(
        std::forward<Args>(args)...));
}

//------------------------------------------------

template<class T>
class AtomicSharedPtr
{
    std::atomic<SharedBase*> pp_ = nullptr;

public:
    ~AtomicSharedPtr()
    {
        acquireUnsafe<T>(pp_.load());
    }

    SharedPtr<T>
    load() const noexcept
    {
        return SharedPtr<T>(pp_.load());
    }

    /** Return an existing or newly constructed object with shared ownership.

        InitFunction must have this equivalent signature:
        @code
        SharedPtr<T>(void)
        @endcode
    */
    template<class InitFunction>
    requires std::is_invocable_r_v<
        SharedPtr<T>, InitFunction>
    SharedPtr<T>
    load(InitFunction const& init)
    {
        if(auto p = pp_.load())
            return SharedPtr<T>(p);
        // If there is a data race, there might
        // be one or more superfluous initializations.
        SharedPtr<T> sp(init());
        if(sp)
        {
            SharedBase* expected = nullptr;
            if(! pp_.compare_exchange_strong(expected, sp.p_))
                return SharedPtr<T>(expected);
            MRDOX_ASSERT(expected == nullptr);
            sp.p_->addref();
        }
        return sp;
    }
};

} // mrdox
} // clang

#endif
