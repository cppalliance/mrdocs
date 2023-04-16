//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_META_LIST_HPP
#define MRDOX_META_LIST_HPP

#include <llvm/Support/ErrorHandling.h>
#include <cstdint>
#include <iterator>
#include <utility>

namespace clang {
namespace mrdox {

/** An append-only list of variants
*/
template<class T>
class List
{
    template<class U>
    struct Item;
    struct Node;
    struct End;

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    using const_pointer = value_type const*;
    using const_reference = value_type const&;

    //--------------------------------------------

    template<bool isConst>
    class iterator_impl;
  
    using iterator = iterator_impl<false>;
    using const_iterator = iterator_impl<true>;

    //--------------------------------------------

    ~List();
    List() noexcept;
    List(List&& other) noexcept;

    // VFALCO unfortunately due to TypedefInfo and
    // EnumInfo, a copy constructor is required.
    // This needs to be fixed.
    List(List const& other);

    List& operator=(List&& other) noexcept;
    List& operator=(List const& other);

    void clear() noexcept;
    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    template<class U, class... Args>
    iterator
    emplace_back(Args&&... args);

    void swap(List& other);

private:
    void append(Node* node);

    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    End end_;
};

//------------------------------------------------

template<class T>
struct List<T>::Node
{
    Node* next;

    virtual ~Node() = default;
    virtual Node* copy(Node* next) = 0;

    explicit
    Node(
        Node* next_) noexcept
        : next(next_)
    {
    }
};

template<class T>
struct List<T>::End : Node
{
    End()
        : Node(nullptr)
    {
    }

    Node* copy(Node*) override
    {
        llvm_unreachable("pure virtual called");
    }
};

template<class T>
template<class U>
struct List<T>::Item : Node
{
    U u;

    T* get() noexcept
    {
        return &u;
    }

    T const* get() const noexcept
    {
        return &u;
    }

    Node* copy(Node* next) override
    {
        return new Item(next, u);
    }

    template<class... Args>
    explicit
    Item(Node* next, Args&&... args)
        : Node(next)
        , u(std::forward<Args>(args)...)
    {
    }
};

template<class T>
template<bool isConst>
class List<T>::iterator_impl
{
    friend class List;

    using value_type = std::conditional_t<isConst, T const, T>;
    using pointer = std::conditional_t<isConst, T const*, T*>;
    using reference = std::conditional_t<isConst, T const&, T&>;
    using node_type = std::conditional_t<isConst, Node const, Node>;

    node_type* it_;

    iterator_impl(node_type* it) noexcept
        : it_(it)
    {
    }

public:
    using size_type = std::size_t;
    using iterator_category =
        std::forward_iterator_tag;

    iterator_impl() = default;

    iterator_impl& operator++() noexcept
    {
        it_ = it_->next();
    }

    iterator_impl operator++(int) noexcept
    {
        auto temp = *this;
        ++temp;
        return temp;
    }

    pointer operator->() const noexcept
    {
        return it_->get();
    }

    reference operator*() const noexcept
    {
        return *it_->get();
    }

    template<bool IsConst0, bool IsConst1>
    friend bool operator==(
        iterator_impl<IsConst0> it0,
        iterator_impl<IsConst1> it1) noexcept
    {
        return it0.it_ == it1.it_;
    }

    template<bool IsConst0, bool IsConst1>
    friend bool operator!=(
        iterator_impl<IsConst0> it0,
        iterator_impl<IsConst1> it1) noexcept
    {
        return !(it0.it_ == it1.it_);
    }
};
    
//--------------------------------------------

template<class T>
List<T>::
~List()
{
    clear();
}

template<class T>
List<T>::
List() noexcept
    : head_(&end_)
    , tail_(&end_)
{
}

template<class T>
List<T>::
List(List&& other) noexcept
{
    if(other.head_ != other.tail_)
    {
        head_ = other.head_;
        tail_ = other.tail_;
        tail_->next = &end_;
        other.head_ = &other.end_;
        other.tail_ = &other.end_;
    }
    else
    {
        head_ = &end_;
        tail_ = &end_;
    }
}

template<class T>
List<T>::
List(List const& other)
    : head_(&end_)
    , tail_(&end_)
{
    for(auto it = other.head_;
            it != &other.end_; it = it->next)
        append(it->copy(&end_));
}

template<class T>
auto
List<T>::
operator=(List&& other) noexcept ->
    List&
{
    List temp(std::move(other));
    temp.swap(*this);
    return *this;
}

template<class T>
auto
List<T>::
operator=(List const& other) ->
    List&
{
    List temp(other);
    this->swap(temp);
    return *this;
}

template<class T>
void
List<T>::
clear() noexcept
{
    for(auto it = head_; it != &end_;)
    {
        auto next = it->next;
        delete it;
        it = next;
    }
}

template<class T>
auto
List<T>::
begin() noexcept ->
    iterator
{
    return head_;
}

template<class T>
auto
List<T>::
end() noexcept ->
    iterator
{
    return end_;
}

#if 0
template<class T>
auto
List<T>::
begin() const noexcept ->
    const_iterator
{
    return head_;
}

template<class T>
auto
List<T>::
end() const noexcept ->
    const_iterator
{
    return end_;
}

template<class T>
auto
List<T>::
cbegin() const noexcept ->
    const_iterator
{
    return head_;
}

template<class T>
auto
List<T>::
cend() const noexcept ->
    const_iterator
{
    return end_;
}
#endif

template<class T>
template<class U, class... Args>
auto
List<T>::
emplace_back(Args&&... args) ->
    iterator
{
    static_assert(std::is_convertible_v<U*, T*>);

    auto item = new Item<U>(
        &end_, std::forward<Args>(args)...);
    append(item);
    return iterator(item);
}

template<class T>
void
List<T>::
swap(List& other)
{
    std::swap(head_, other.head_);
    std::swap(tail_, other.tail_);
    if(head_ == tail_)
        head_->next = &end_;
    tail_->next = &end_;
}

template<class T>
void
List<T>::
append(Node* node)
{
    if(head_ == &end_)
    {
        head_ = node;
        tail_ = node;
    }
    else
    {
        tail_->next = node;
        tail_ = node;
    }
}

} // mrdox
} // clang

#endif
