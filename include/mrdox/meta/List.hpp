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
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <typeinfo>
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

    iterator begin() noexcept;
    iterator end() noexcept;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    const_iterator cbegin() const noexcept;
    const_iterator cend() const noexcept;

    bool empty() const noexcept;
    std::size_t size() const noexcept;
    void clear() noexcept;

    std::strong_ordering
    operator<=>(List const& other) const noexcept;

    template<class Pred>
    bool erase_first_of_if(Pred&& pred) noexcept;

    template<class U>
    iterator emplace_back(U&& u);

    void swap(List& other);

private:
    void append(Node* node);

    std::size_t size_ = 0;
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
    virtual T* get() noexcept = 0;
    virtual T const* get() const noexcept = 0;
    virtual std::type_info const* id() const noexcept = 0;
    virtual std::strong_ordering compare(
        Node const*) const noexcept = 0;
    virtual Node* copy(Node* next) const = 0;

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

    T* get() noexcept override
    {
        unreachable();
        return nullptr;
    }

    T const* get() const noexcept override
    {
        unreachable();
        return nullptr;
    }

    std::type_info const* id() const noexcept override
    {
        unreachable();
        return nullptr;
    }

    std::strong_ordering
    compare(Node const*) const noexcept override
    {
        unreachable();
        return std::strong_ordering::equivalent;
    }

    Node* copy(Node*) const override
    {
        unreachable();
        return nullptr;
    }

private:
    [[noreturn]] void unreachable() const
    {
        llvm_unreachable("pure virtual called");
    }
};

template<class T>
template<class U>
struct List<T>::Item : Node
{
    static constexpr std::type_info const* id_ = &typeid(U);

    template<class... Args>
    explicit
    Item(Node* next, Args&&... args)
        : Node(next)
        , u_(std::forward<Args>(args)...)
    {
    }

    T* get() noexcept override
    {
        return &u_;
    }

    T const* get() const noexcept override
    {
        return &u_;
    }

    std::type_info const* id() const noexcept override
    {
        return id_;
    }

    std::strong_ordering
    compare(Node const* other) const noexcept override
    {
        if(std::less<void const*>()(id_, other->id()))
            return std::strong_ordering::less;
        if(std::less<void const*>()(other->id(), id_))
            return std::strong_ordering::greater;
        return u_ <=> *static_cast<U const*>(other->get());
    }

    Node* copy(Node* next) const override
    {
        return new Item(next, u_);
    }

private:
    U u_;
};

template<class T>
template<bool isConst>
class List<T>::iterator_impl
{
    friend class List;

    using value_type = std::conditional_t<isConst, T const, T>;
    using pointer    = std::conditional_t<isConst, T const*, T*>;
    using reference  = std::conditional_t<isConst, T const&, T&>;
    using node_type  = std::conditional_t<isConst, Node const, Node>;

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
        it_ = it_->next;
        return *this;
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
        return *(it_->get());
    }

    template<bool IsConst_>
    bool operator==(
        iterator_impl<IsConst_> it) const noexcept
    {
        return it_ == it.it_;
    }

    template<bool IsConst_>
    bool operator!=(
        iterator_impl<IsConst_> it) const noexcept
    {
        return it_ != it.it_;
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
    if(! other.empty())
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
    return &end_;
}

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
    return &end_;
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
    return &end_;
}

template<class T>
bool
List<T>::
empty() const noexcept
{
    return head_ == &end_;
}

template<class T>
std::size_t
List<T>::
size() const noexcept
{
    return size_;
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
std::strong_ordering
List<T>::
operator<=>(
    List const& other) const noexcept
{
    if(size() < other.size())
        return std::strong_ordering::less;
    else if(size() > other.size())
        return std::strong_ordering::greater;
    auto it0 = head_;
    auto it1 = other.head_;
    while(it0 != &end_)
    {
        auto result = it0->compare(it1);
        if(result != std::strong_ordering::equivalent)
            return result;
        ++it0;
        ++it1;
    }
    return std::strong_ordering::equivalent;
}

template<class T>
template<class Pred>
bool
List<T>::
erase_first_of_if(
    Pred&& pred) noexcept
{
    if(empty())
        return false;
    if(pred(*head_->get()))
    {
        head_ = head_->next;
        if(head_ == &end_)
            tail_ = &end_;
        --size_;
        return true;
    }
    auto prev = head_;
    auto it = head_->next;
    while(it != &end_)
    {
        if(pred(*it->get()))
        {
            prev->next = it->next;
            if(it == tail_)
                tail_ = prev;
            --size_;
            return true;
        }
        prev = it;
        it = it->next;
    }
    return false;
}

template<class T>
template<class U>
auto
List<T>::
emplace_back(U&& u) ->
    iterator
{
    static_assert(std::is_convertible_v<U*, T*>);

    auto item = new Item<U>(&end_, std::forward<U>(u));
    append(item);
    return iterator(item);
}

template<class T>
void
List<T>::
swap(List& other)
{
    if(other.empty())
    {
        if(empty())
            return;

        other.head_ = head_;
        other.tail_ = tail_;
        other.tail_->next = &other.end_;
        head_ = &end_;
        tail_ = &end_;
    }
    else if(empty())
    {
        head_ = other.head_;
        tail_ = other.tail_;
        tail_->next = &end_;
        other.head_ = &other.end_;
        other.tail_ = other.tail_;
    }
    else
    {
        std::swap(head_, other.head_);
        std::swap(tail_, other.tail_);
        tail_->next = &end_;
        other.tail_->next = &other.end_;
    }
    std::swap(size_, other.size_);
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
    ++size_;
}

} // mrdox
} // clang

#endif
