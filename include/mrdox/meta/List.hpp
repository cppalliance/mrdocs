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
    struct Node
    {
        Node* next;

        virtual ~Node() = default;

        explicit
        Node(
            Node* next_) noexcept
            : next(next_)
        {
        }
    };

    template<class U>
    struct Item : Node
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

        template<class... Args>
        explicit
        Item(Node* next, Args&&... args)
            : Node(next)
            , u(std::forward<Args>(args)...)
        {
        }
    };

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
    struct member_types
    {
        using value_type = T;
        using pointer = value_type*;
        using reference = value_type&;
    protected:
        using node_type = Node;
    };

    template<>
    struct member_types<true>
    {
        using value_type = T const;
        using pointer = value_type const*;
        using reference = value_type const&;
    protected:
        using node_type = Node const;
    };

    template<bool isConst>
    class iterator_impl
        : public member_types<isConst>
    {
        friend class List;

        using node_type =
            member_types<isConst>::node_type;

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
    
    using iterator = iterator_impl<false>;
    using const_iterator = iterator_impl<true>;

    //--------------------------------------------

    ~List()
    {
        for(auto it = head_; it != &end_;)
        {
            auto next = it->next;
            delete it;
            it = next;
        }
    }

    List()
        : head_(&end_)
        , tail_(&end_)
        , end_(nullptr)
    {
    }

    iterator begin() noexcept
    {
        return head_;
    }

    iterator end() noexcept
    {
        return end_;
    }

    const_iterator begin() const noexcept
    {
        return head_;
    }

    const_iterator end() const noexcept
    {
        return end_;
    }

    const_iterator cbegin() const noexcept
    {
        return head_;
    }

    const_iterator cend() const noexcept
    {
        return end_;
    }

    template<class U, class... Args>
    iterator
    emplace_back(Args&&... args)
    {
        static_assert(std::is_convertible<U*, T*>);

        auto item = new Item<U>(
            &end_, std::forward<Args>(args)...);
        if(head_ == &end_)
        {
            head_ = item;
            tail_ = item;
        }
        else
        {
            tail_->next = item;
            tail_ = item;
        }
        return iterator(item);
    }

private:
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    Node end_;
};

} // mrdox
} // clang

#endif
