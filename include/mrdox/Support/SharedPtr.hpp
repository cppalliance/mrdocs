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
#include <utility>

namespace clang {
namespace mrdox {

template<class T> class SharedPtr;
template<class T, class... Args>
SharedPtr<T> makeShared(Args&&...);

//------------------------------------------------

/** The native object type used in SharedPtr.

    Pointers to objects of this type are used to
    transfer ownership outside of the shared
    pointer container. If used incorrectly, this
    can violate type-safety and break invariants.
*/
class SharedImpl
{
private:
    std::atomic<std::size_t> refs = 1;
    virtual ~SharedImpl() = default;
    SharedImpl() noexcept = default;
    virtual void* get() noexcept = 0;
    SharedImpl* addref() noexcept;
    void release() noexcept;

    template<class T> friend class SharedPtr;
    template<class T, class... Args>
    friend SharedPtr<T> makeShared(Args&&...);
};

//------------------------------------------------

template<class T> class AtomicSharedPtr;
template<class T>
SharedPtr<T> makeSharedImpl(SharedImpl*) noexcept;

/** A simple smart pointer container with shared ownership.

    Shared ownership of the managed object
    can be retrieved from the container as an
    implementation-defined opaque pointer. This
    weakens type-safety but allows convenient
    interfacing with C libraries.
*/
template<class T>
class SharedPtr
{
    SharedImpl* p_ = nullptr;

    static_assert(! std::convertible_to<T*, SharedImpl*>);

    template<class U> friend class SharedPtr;
    template<class U> friend class AtomicSharedPtr;

    explicit SharedPtr(SharedImpl*) noexcept;
    SharedPtr(SharedImpl*, int) noexcept;

public:
    ~SharedPtr();
    SharedPtr() noexcept = default;
    SharedPtr(SharedPtr&& other) noexcept;
    SharedPtr(SharedPtr const& other) noexcept;
    SharedPtr& operator=(SharedPtr&& other) noexcept;
    SharedPtr& operator=(SharedPtr const& other) noexcept;

    template<class U>
    requires std::convertible_to<U*, T*>
    SharedPtr(SharedPtr<U>&& other) noexcept;

    template<class U>
    requires std::convertible_to<U*, T*>
    SharedPtr(SharedPtr<U> const& other) noexcept;

    template<class U>
    requires std::convertible_to<U*, T*>
    SharedPtr& operator=(SharedPtr<U>&& other) noexcept;

    template<class U>
    requires std::convertible_to<U*, T*>
    SharedPtr& operator=(SharedPtr<U> const& other) noexcept;

    T* get() const noexcept;
    T* operator->() const noexcept;
    T& operator*() const noexcept;
    explicit operator bool() const noexcept;

    /** Return a pointer to a newly created object.

        The caller receives shared ownership.

        @tparam U The type of object to create.

        @param args... Arguments forwarded to
        the constructor.
    */
    template<class U, class... Args>
    friend SharedPtr<U> makeShared(Args&&... args);

    /** Return a new pointer with shared ownership.
    */
    template<class U>
    friend
    SharedPtr<U>
    makeSharedImpl(SharedImpl*) noexcept;

    // unsafe access

    /** Return the implementation.

        Ownership is not transferred; the caller
        receives a reference to the managed object.
    */
    SharedImpl* getImpl() const noexcept;

    /** Return the implementation.

        The caller receives shared ownership of the
        managed object. Every call to @ref sharedImpl
        must be balanced by an eventual, corresponding
        call to @ref unshareImpl which refers to the
        same managed object, or else the behavior of
        the program is undefined.
    */
    SharedImpl* shareImpl() const noexcept;

    /** Release ownership of the implementation

        This function releases the shared ownership
        of exactly one previous call to @ref shareImpl.
    */
    void unshareImpl() const noexcept;

    // swap

    void swap(SharedPtr& rhs) noexcept;

    friend void swap(
        SharedPtr& lhs, SharedPtr& rhs) noexcept
    {
        lhs.swap(rhs);
    }
};

//------------------------------------------------

inline
auto
SharedImpl::
SharedImpl::
addref() noexcept ->
    SharedImpl*
{
    ++refs;
    return this;
}

inline
void
SharedImpl::
SharedImpl::
release() noexcept
{
    if(--refs > 0)
        return;
    delete this;
}

//------------------------------------------------

// ownership of p is transferred
template<class T>
SharedPtr<T>::
SharedPtr(
    SharedImpl* p) noexcept
    : p_(p)
{
}

// ownership of p is shared
template<class T>
SharedPtr<T>::
SharedPtr(
    SharedImpl* p, int) noexcept
    : p_(p->addref())
{
}

template<class T>
SharedPtr<T>::
~SharedPtr()
{
    if( p_)
        p_->release();
}

template<class T>
SharedPtr<T>::
SharedPtr(
    SharedPtr&& other) noexcept
    : p_(other.p_)
{
    other.p_ = nullptr;
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
    SharedPtr&& other) noexcept ->
    SharedPtr&
{
    if(this == &other)
        return *this;
    if( p_)
        p_->release();
    p_ = other.p_;
    other.p_ = nullptr;
    return *this;
}

template<class T>
auto
SharedPtr<T>::
operator=(
    SharedPtr const& other) noexcept ->
    SharedPtr&
{
    if( other.p_)
        other.p_->addref();
    if( p_)
        p_->release();
    p_ = other.p_;
    return *this;
}

template<class T>
template<class U>
requires std::convertible_to<U*, T*>
SharedPtr<T>::
SharedPtr(
    SharedPtr<U>&& other) noexcept
    : p_(other.p_)
{
    other.p_ = nullptr;
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
    SharedPtr<U>&& other) noexcept ->
    SharedPtr&
{
    if( p_)
        p_->release();
    p_ = other.p_;
    other.p_ = nullptr;
    return *this;
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
    if( other.p_)
        other.p_->addref();
    if( p_)
        p_->release();
    p_ = other.p_;
    return *this;
}

template<class T>
T*
SharedPtr<T>::
get() const noexcept
{
    if(p_)
        return static_cast<T*>(p_->get());
    return nullptr;
}

template<class T>
T*
SharedPtr<T>::
operator->() const noexcept
{
    MRDOX_ASSERT(p_ != nullptr);
    return get();
}

template<class T>
T&
SharedPtr<T>::
operator*() const noexcept
{
    MRDOX_ASSERT(p_ != nullptr);
    return *get();
}

template<class T>
SharedPtr<T>::
operator bool() const noexcept
{
    return p_ != nullptr;
}

// ownership is not transferred
template<class T>
SharedImpl*
SharedPtr<T>::
getImpl() const noexcept
{
    MRDOX_ASSERT(p_ != nullptr);
    return p_;
}

// ownership is transferred
template<class T>
SharedImpl*
SharedPtr<T>::
shareImpl() const noexcept
{
    MRDOX_ASSERT(p_ != nullptr);
    return p_->addref();
}

// ownership is transferred
template<class T>
void
SharedPtr<T>::
unshareImpl() const noexcept
{
    MRDOX_ASSERT(p_ != nullptr);
    p_->release();
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

template<class T, class... Args>
SharedPtr<T>
makeShared(Args&&... args)
{
    struct Impl : SharedImpl
    {
        T u;

        explicit Impl(Args&&... args)
            : u(std::forward<Args>(args)...)
        {
        }

        void* get() noexcept override
        {
            return &u;
        }
    };

    return SharedPtr<T>(new Impl(
        std::forward<Args>(args)...));
}

template<class T>
SharedPtr<T>
makeSharedImpl(
    SharedImpl* p) noexcept
{
    return SharedPtr<T>(p, 0);
}

//------------------------------------------------

template<class T>
class AtomicSharedPtr
{
    std::atomic<SharedImpl*> mutable pp_ = nullptr;

public:
    ~AtomicSharedPtr()
    {
        SharedImpl* p = pp_.load();
        if(! p)
            return;
        makeSharedImpl<T>(p).unshareImpl();
    }

    /** Return an existing or newly constructed object with shared ownership.

        InitFunction must have this equivalent signature:
        @code
        SharedPtr<T>(void)
        @endcode

        The initialization function may not
        return a null shared pointer.
    */
    template<class InitFunction>
    requires std::is_invocable_r_v<
        SharedPtr<T>, InitFunction>
    auto
    load(InitFunction const& init) const ->
        SharedPtr<T>
    {
        if(auto p = pp_.load())
            return makeSharedImpl<T>(p);
        // If there is a data race, there might
        // be one or more superfluous initializations.
        SharedPtr<T> sp(init());
        MRDOX_ASSERT(sp);
        SharedImpl* expected = nullptr;
        if(! pp_.compare_exchange_strong(
            expected, sp.getImpl()))
        {
            return makeSharedImpl<T>(expected);
        }
        else
        {
            MRDOX_ASSERT(expected == nullptr);
            sp.shareImpl(); 
        }
        return sp;
    }
};

} // mrdox
} // clang

#endif
