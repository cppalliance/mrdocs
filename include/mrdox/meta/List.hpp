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

#include <mrdox/Platform.hpp>
#include <algorithm>
#include <compare>
#include <cstdint>
#include <iterator>
#include <memory>
#include <typeinfo>
#include <utility>

namespace clang {
namespace mrdox {

template<class T>
using compare_result_t =
    typename std::conditional_t<
        std::three_way_comparable<T>,
        std::compare_three_way_result<T>,
        std::type_identity<void>
    >::type;

//------------------------------------------------

struct ListNodes
{
    std::size_t size = 0;
    void* head = nullptr;
    void* tail = nullptr;
};

template<class Order>
class ListBase
{
protected:
    struct Node;

    template<class U>
    struct Item;
};

//------------------------------------------------

template<class Order>
struct ListBase<Order>::Node
{
    Node* next;

    virtual ~Node() = default;
    virtual void* get() noexcept { return nullptr; }
    virtual void const* get() const noexcept { return nullptr; }
    virtual std::type_info const* id() const noexcept { return nullptr; }
    virtual Node* copy(Node* next) const { return nullptr; }
    virtual Order compare(Node const*) const noexcept
    {
        return static_cast<Order>(std::strong_ordering::equivalent);
    }

    explicit
    Node(
        Node* next_ = nullptr) noexcept
        : next(next_)
    {
    }
};

//------------------------------------------------

/** An append-only list of variants
*/
template<class T>
class List : public ListBase<compare_result_t<T>>
{
    template<class U>
    friend class List;

    template<class U>
    using Item = typename ListBase<compare_result_t<T>>::template Item<U>;
    using Node = typename ListBase<compare_result_t<T>>::Node;

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    using const_pointer = value_type const*;
    using const_reference = value_type const&;
    using compare_result = compare_result_t<T>;

    //--------------------------------------------

    template<bool isConst>
    class iterator_impl;

    using iterator = iterator_impl<false>;
    using const_iterator = iterator_impl<true>;

    //--------------------------------------------

    ~List();
    List() noexcept;
    explicit List(ListNodes&&) noexcept;

    template<class U>
    List(List<U>&& other) noexcept;
    List(List&& other) noexcept;
    List& operator=(List&& other) noexcept;

    // VFALCO unfortunately due to TypedefInfo and
    // EnumInfo, a copy constructor is required.
    // This needs to be fixed.
    template<class U>
    List(List<U> const& other) requires std::copyable<T>;
    List(List const& other) requires std::copyable<T>;
    List& operator=(List const& other) requires std::copyable<T>;

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
    ListNodes extractNodes() noexcept;
    void spliceBack(ListNodes&& nodes) noexcept;

    compare_result compare(List const& other) const noexcept
        requires std::three_way_comparable<T>;
    compare_result operator<=>(List const& other) const noexcept
        requires std::three_way_comparable<T>;
    bool operator==(List const& other) const noexcept
        requires std::equality_comparable<T>;

    template<class U = T, class Pred>
    std::shared_ptr<U const>
    extract_first_of(Pred&& pred) noexcept;

    template<class U>
    U& emplace_back(U&& u);

    template<class U>
    void splice_back(List<U>&& other) noexcept;

    void swap(List& other);

private:
    void append(Node* node);

    std::size_t size_;
    Node* head_;
    Node* tail_;
    Node end_;
};

//------------------------------------------------

template<class Order>
template<class U>
struct ListBase<Order>::Item : Node
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

    Order
    compare(Node const* other) const noexcept override
    {
        if constexpr(std::is_same_v<Order, void>)
            return;
        else
        {
            if(std::less<void const*>()(id_, other->id()))
                return Order::less;
            if(std::less<void const*>()(other->id(), id_))
                return Order::greater;
            return u_ <=> *reinterpret_cast<U const*>(other->get());
        }
    }

    Node* copy(Node* next) const override
    {
        if constexpr(std::copyable<U>)
            return new Item(next, u_);
        else
            return nullptr;
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
List<T>::
List(ListNodes&& nodes) noexcept
    : size_(nodes.size)
{
    if(! nodes.head)
    {
        head_ = &end_;
        tail_ = &end_;
        return;
    }

    head_ = reinterpret_cast<Node*>(nodes.head);
    tail_ = reinterpret_cast<Node*>(nodes.tail);
    tail_->next = &end_;
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
auto
List<T>::
operator=(List&& other) noexcept ->
    List&
{
    List temp(std::move(other));
    temp.swap(*this);
    return *this;
}

//------------------------------------------------

template<class T>
template<class U>
List<T>::
List(List<U> const& other)
    requires std::copyable<T>
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
    requires std::copyable<T>
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
operator=(List const& other) ->
    List &
    requires std::copyable<T>
{
    List temp(other);
    this->swap(temp);
    return *this;
}

//------------------------------------------------

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
ListNodes
List<T>::
extractNodes() noexcept
{
    if(empty())
        return {};
    ListNodes result{ size_, head_, tail_ };
    size_ = 0;
    head_ = &end_;
    tail_ = &end_;
    return result;
}

template<class T>
void
List<T>::
spliceBack(ListNodes&& nodes) noexcept
{
    splice_back(List<T>(std::move(nodes)));
}

template<class T>
auto
List<T>::
compare(
    List const& other) const noexcept ->
        compare_result
    requires std::three_way_comparable<T>
{
    if (auto r = size() <=> other.size(); r != 0)
        return r;
    auto it0 = head_;
    auto it1 = other.head_;
    while(it0 != &end_)
    {
        if (auto r = it0->compare(it1); r != 0)
            return r;
        it0 = it0->next;
        it1 = it1->next;
    }
    return std::strong_ordering::equal;
}

template<class T>
auto
List<T>::
operator<=>(
    List const& other) const noexcept ->
        compare_result
    requires std::three_way_comparable<T>
{
    return compare(other);
}

template<class T>
bool
List<T>::
operator==(
    List const& other) const noexcept
    requires std::equality_comparable<T>
{
    return compare(other) == compare_result::equal;
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
        auto node = head_;
        auto result = std::make_shared<U>(std::move(
            *reinterpret_cast<U*>(head_->get())));
        head_ = head_->next;
        if(head_ == &end_)
            tail_ = &end_;
        --size_;
        delete node;
        return result;
    }
    auto prev = head_;
    auto it = head_->next;
    while(it != &end_)
    {
        if(pred(*reinterpret_cast<pointer>(it->get())))
        {
            auto node = it;
            auto result = std::make_shared<U>(std::move(
                *reinterpret_cast<U*>(it->get())));
            prev->next = it->next;
            if(it == tail_)
                tail_ = prev;
            --size_;
            delete node;
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
    auto item = new Item<U>(&end_, std::forward<U>(u));
    append(item);
    return item->u_;
}

template<class T>
template<class U>
void
List<T>::
splice_back(
    List<U>&& other) noexcept
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
