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
#include <memory>
#include <typeinfo>
#include <utility>

namespace clang {
namespace mrdox {

//------------------------------------------------

class ListBase
{
protected:
    template<class U>
    struct Item;
    struct Node;
    struct End;
};

struct ListBase::Node
{
    Node* next;

    virtual ~Node() = default;
    virtual void* get() noexcept = 0;
    virtual void const* get() const noexcept = 0;
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

struct ListBase::End : Node
{
    End()
        : Node(nullptr)
    {
    }

    void* get() noexcept override
    {
        unreachable();
        return nullptr;
    }

    void const* get() const noexcept override
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

//------------------------------------------------

/** An append-only list of variants
*/
template<class T>
class List : public ListBase
{
    template<class U>
    friend class List;

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

    template<class U>
    List(List<U>&& other) noexcept;
    List(List&& other) noexcept;

    // VFALCO unfortunately due to TypedefInfo and
    // EnumInfo, a copy constructor is required.
    // This needs to be fixed.
    template<class U>
    List(List<U> const& other);
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
    T const& back() const noexcept;
    T& back() noexcept;

    void clear() noexcept;

    std::strong_ordering
    operator<=>(List const& other) const noexcept;

    template<class U = T, class Pred>
    std::shared_ptr<U const>
    extract_first_of(Pred&& pred) noexcept;

    template<class U>
    U& emplace_back(U&& u);

    template<class U>
    void splice_back(List<U>& other);

    void swap(List& other);

private:
    void append(Node* node);

    std::size_t size_;
    Node* head_;
    Node* tail_;
    End end_;
};

//------------------------------------------------

template<class U>
struct ListBase::Item : Node
{
    static constexpr std::type_info const* id_ = &typeid(U);

    template<class... Args>
    explicit
    Item(Node* next, Args&&... args)
        : Node(next)
        , u_(std::forward<Args>(args)...)
    {
    }

    void* get() noexcept override
    {
        return &u_;
    }

    void const* get() const noexcept override
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
        return u_ <=> *reinterpret_cast<U const*>(other->get());
    }

    Node* copy(Node* next) const override
    {
        return new Item(next, u_);
    }

    U u_;
};

//------------------------------------------------

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
        return reinterpret_cast<pointer>(it_->get());
    }

    reference operator*() const noexcept
    {
        return *operator->();
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
    : size_(0)
    , head_(&end_)
    , tail_(&end_)
{
}

template<class T>
template<class U>
List<T>::
List(List<U>&& other) noexcept
    : size_(other.size_)
{
    if(size_ == 0)
    {
        head_ = &end_;
        tail_ = &end_;
        return;
    }

    head_ = other.head_;
    tail_ = other.tail_;
    tail_->next = &end_;
    other.size_ = 0;
    other.head_ = &other.end_;
    other.tail_ = &other.end_;
}

template<class T>
List<T>::
List(List&& other) noexcept
    : size_(other.size_)
{
    if(size_ == 0)
    {
        head_ = &end_;
        tail_ = &end_;
        return;
    }

    head_ = other.head_;
    tail_ = other.tail_;
    tail_->next = &end_;
    other.size_ = 0;
    other.head_ = &other.end_;
    other.tail_ = &other.end_;
}

template<class T>
template<class U>
List<T>::
List(List<U> const& other)
    : size_(0)
    , head_(&end_)
    , tail_(&end_)
{
    for(auto it = other.head_;
            it != &other.end_; it = it->next)
        append(it->copy(&end_));
}

template<class T>
List<T>::
List(List const& other)
    : size_(0)
    , head_(&end_)
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
T const&
List<T>::
back() const noexcept
{
    return *reinterpret_cast<T const*>(tail_->get());
}

template<class T>
T&
List<T>::
back() noexcept
{
    return *reinterpret_cast<T*>(tail_->get());
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
    size_ = 0;
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
template<class U, class Pred>
std::shared_ptr<U const>
List<T>::
extract_first_of(
    Pred&& pred) noexcept
{
    if(empty())
        return nullptr;
    if(pred(*reinterpret_cast<pointer>(head_->get())))
    {
        auto result = std::make_shared<U>(std::move(
            *reinterpret_cast<U*>(head_->get())));
        head_ = head_->next;
        if(head_ == &end_)
            tail_ = &end_;
        --size_;
        return result;
    }
    auto prev = head_;
    auto it = head_->next;
    while(it != &end_)
    {
        if(pred(*reinterpret_cast<pointer>(it->get())))
        {
            auto result = std::make_shared<U>(std::move(
                *reinterpret_cast<U*>(it->get())));
            prev->next = it->next;
            if(it == tail_)
                tail_ = prev;
            --size_;
            return result;
        }
        prev = it;
        it = it->next;
    }
    return nullptr;
}

template<class T>
template<class U>
auto
List<T>::
emplace_back(U&& u) ->
    U&
{
    static_assert(std::is_convertible_v<U*, T*>);

    auto item = new Item<U>(&end_, std::forward<U>(u));
    append(item);
    return item->u_;
}

template<class T>
template<class U>
void
List<T>::
splice_back(
    List<U>& other)
{
    if(other.empty())
        return;
    if(empty())
    {
        *this = std::move(other);
        return;
    }
    size_ += other.size_;
    tail_->next = other.head_;
    tail_ = other.tail_;
    tail_->next = &end_;
    other.size_ = 0;
    other.head_ = &other.end_;
    other.tail_ = &other.end_;
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

        std::swap(size_, other.size_);
        other.head_ = head_;
        other.tail_ = tail_;
        other.tail_->next = &other.end_;
        head_ = &end_;
        tail_ = &end_;
        return;
    }
    if(empty())
    {
        std::swap(size_, other.size_);
        head_ = other.head_;
        tail_ = other.tail_;
        tail_->next = &end_;
        other.head_ = &other.end_;
        other.tail_ = &other.end_;
        return;
    }

    std::swap(size_, other.size_);
    std::swap(head_, other.head_);
    std::swap(tail_, other.tail_);
    tail_->next = &end_;
    other.tail_->next = &other.end_;
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
