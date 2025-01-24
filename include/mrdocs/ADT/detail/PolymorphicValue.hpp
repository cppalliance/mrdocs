//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_ADT_DETAIL_POLYMORPHICVALUE_HPP
#define MRDOCS_API_ADT_DETAIL_POLYMORPHICVALUE_HPP

#include <mrdocs/Support/Assert.hpp>
#include <type_traits>
#include <utility>
#include <typeinfo>
#include <functional>

namespace clang::mrdocs::detail {

// Abstract base class for control blocks
// A control block is a block of storage to hold the derived
// object and a virtual function to delete it.
// The object is stored in a unique_ptr so it already contains
// the delete operation.
// The copies are implemented with clone().
template <class Base>
struct PolymorphicStorage
{
    constexpr virtual ~PolymorphicStorage() = default;

    // Clone the control block.
    // Deallocation happens though the destroy function implemented
    // in the derived class.
    constexpr virtual
    PolymorphicStorage*
    clone() const = 0;

    constexpr virtual Base* ptr() = 0;
};

// Control block that holds the object directly as a member
template <class Base, class Derived = Base>
class DirectPolymorphicStorage final
    : public PolymorphicStorage<Base>
{
    static_assert(!std::is_reference_v<Derived>);
    Derived u_;

public:
    constexpr ~DirectPolymorphicStorage() override = default;

    template <class... Args>
    constexpr explicit
    DirectPolymorphicStorage(Args&&... args)
        : u_(Derived(std::forward<Args>(args)...)) {}

    constexpr
    PolymorphicStorage<Base>*
    clone() const override
    {
        return new DirectPolymorphicStorage(*this);
    }

    constexpr
    Base*
    ptr() override
    {
        return std::addressof(u_);
    }
};

// Control block that holds the object in a unique_ptr
// and stores a copier function
// This is the main control block type used
// by the PolymorphicValue class
template <class Base, class Derived, class Copier, class Deleter>
class PointerPolymorphicStorage final
    : public PolymorphicStorage<Base>
    , public Copier
{
    Derived* p_{nullptr};
    Copier c_;
    Deleter d_;

public:
    // Virtual destructor
    constexpr
    ~PointerPolymorphicStorage() override
    {
        d_(p_);
    }

    constexpr
    explicit
    PointerPolymorphicStorage(Derived* u, Copier c, Deleter d)
        : p_(u), c_(std::move(c)), d_(std::move(d)) {}

    constexpr
    PolymorphicStorage<Base>*
    clone() const override
    {
        MRDOCS_ASSERT(p_);
        return new PointerPolymorphicStorage(c_(*p_), c_, d_);
    }

    constexpr
    Base*
    ptr() override
    {
        return p_;
    }
};

// Control block that delegates to another control block
// This is necessary to implement conversions
template <class Base, class Upstream>
class DelegatingPolymorphicStorage final
    : public PolymorphicStorage<Base>
{
    PolymorphicStorage<Upstream>* delegate_{nullptr};

public:
    // virtual destructor
    constexpr
    ~DelegatingPolymorphicStorage() override
    {
        delete delegate_;
    }

    constexpr
    explicit
    DelegatingPolymorphicStorage(PolymorphicStorage<Upstream>* b)
        : delegate_(std::move(b))
    {}

    constexpr
    PolymorphicStorage<Base>*
    clone() const override
    {
        return new DelegatingPolymorphicStorage(delegate_->clone());
    }

    constexpr
    Base*
    ptr() override {
        return dynamic_cast<Base*>(delegate_->ptr());
    }
};

// A function to compare two polymorphic objects that
// store the same derived type
template <class Base>
struct VisitCompareFn {
    Base const& rhs;

    template <class Derived>
    auto
    operator()(Derived const& lhsDerived)
    {
        auto const& rhsDerived = dynamic_cast<Derived const&>(rhs);
        return lhsDerived <=> rhsDerived;
    }
};

/** Determines if a type can be visited with VisitCompareFn
 */
template <class Base>
concept CanVisitCompare = requires(Base const& b)
{
    visit(b, VisitCompareFn<Base>{b});
};

} // clang::mrdocs

#endif
