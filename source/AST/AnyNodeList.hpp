//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_AST_ANYNODELIST_HPP
#define MRDOX_AST_ANYNODELIST_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Support/Error.hpp>
#include <mrdox/Metadata/Javadoc.hpp>
#include <mrdox/ADT/AnyList.hpp>

namespace clang {
namespace mrdox {

/** Helper for converting bitcode into lists of Javadoc nodes.
*/
class AnyNodeList
{
public:
    struct Nodes
    {
        virtual ~Nodes() = default;
        virtual Error appendChild(Javadoc::Kind kind) = 0;
        virtual Error setStyle(Javadoc::Style style) = 0;
        virtual Error setString(llvm::StringRef string) = 0;
        virtual Error setAdmonish(Javadoc::Admonish admonish) = 0;
        virtual Error setDirection(Javadoc::ParamDirection direction) = 0;
        virtual AnyListNodes extractNodes() = 0;
        virtual void spliceBack(AnyListNodes&& nodes) noexcept = 0;
    };

    ~AnyNodeList();

    explicit
    AnyNodeList(
        AnyNodeList*& stack) noexcept;

    AnyNodeList*& stack() const noexcept
    {
        return stack_;
    }

    Nodes& getNodes();
    bool isTopLevel() const noexcept;
    Error setKind(Javadoc::Kind kind);

    template<class T>
    Error spliceInto(AnyList<T>& nodes);
    Error spliceIntoParent();

private:
    template<class T>
    struct NodesImpl : Nodes
    {
        AnyList<T> list;

        Error appendChild(Javadoc::Kind kind) override;
        Error setStyle(Javadoc::Style style) override;
        Error setString(llvm::StringRef string) override;
        Error setAdmonish(Javadoc::Admonish admonish) override;
        Error setDirection(Javadoc::ParamDirection direction) override;
        AnyListNodes extractNodes() override;
        void spliceBack(AnyListNodes&& nodes) noexcept override;
    };

    AnyNodeList* prev_;
    AnyNodeList*& stack_;
    Nodes* nodes_ = nullptr;
};

//------------------------------------------------

inline
AnyNodeList::
~AnyNodeList()
{
    if(nodes_)
        delete nodes_;
    stack_ = prev_;
}

inline
AnyNodeList::
AnyNodeList(
    AnyNodeList*& stack) noexcept
    : prev_(stack)
    , stack_(stack)
{
    stack_ = this;
}

inline
auto
AnyNodeList::
getNodes() ->
    Nodes&
{
    if(nodes_)
        return *nodes_;

    struct ErrorNodes : Nodes
    {
        Error appendChild(Javadoc::Kind) override
        {
            return Error("kind is missing");
        }

        Error setStyle(Javadoc::Style) override
        {
            return Error("kind is missing");
        }

        Error setString(llvm::StringRef) override
        {
            return Error("kind is missing");
        }

        Error setAdmonish(Javadoc::Admonish) override
        {
            return Error("kind is missing");
        }

        Error setDirection(Javadoc::ParamDirection) override
        {
            return Error("kind is missing");
        }

        AnyListNodes extractNodes() override
        {
            return {};
        }

        void spliceBack(AnyListNodes&&) noexcept override
        {
        }
    };
    static ErrorNodes nodes;
    return nodes;
}

inline
bool
AnyNodeList::
isTopLevel() const noexcept
{
    return prev_ == nullptr;
}

inline
Error
AnyNodeList::
setKind(
    Javadoc::Kind kind)
{
    if(nodes_ != nullptr)
        return Error("kind already set");
    switch(kind)
    {
    case Javadoc::Kind::block:
        nodes_ = new NodesImpl<Javadoc::Block>();
        break;
    case Javadoc::Kind::text:
        nodes_ = new NodesImpl<Javadoc::Text>();
        break;
    default:
        return Error("wrong or unknown kind");
    }
    return Error::success();
}

template<class T>
Error
AnyNodeList::
spliceInto(
    AnyList<T>& nodes)
{
    if(nodes_ == nullptr)
        return Error("splice without nodes");

    // splice `*nodes_` to the end of `nodes`
    nodes.spliceBack(nodes_->extractNodes());

    return Error::success();
}

inline
Error
AnyNodeList::
spliceIntoParent()
{
    if(prev_ == nullptr)
        return Error("splice without parent");

    // splice `*nodes_` to the end of `*prev_->nodes_`
    prev_->nodes_->spliceBack(nodes_->extractNodes());

    return Error::success();
}

//------------------------------------------------

template<class T>
Error
AnyNodeList::
NodesImpl<T>::
appendChild(
    Javadoc::Kind kind)
{
    switch(kind)
    {
    case Javadoc::Kind::text:
        Javadoc::append(list, Javadoc::Text());
        return Error::success();
    case Javadoc::Kind::styled:
        Javadoc::append(list, Javadoc::StyledText());
        return Error::success();
    case Javadoc::Kind::paragraph:
        Javadoc::append(list, Javadoc::Paragraph());
        return Error::success();
    case Javadoc::Kind::brief:
        Javadoc::append(list, Javadoc::Brief());
        return Error::success();
    case Javadoc::Kind::admonition:
        Javadoc::append(list, Javadoc::Admonition());
        return Error::success();
    case Javadoc::Kind::code:
        Javadoc::append(list, Javadoc::Code());
        return Error::success();
    case Javadoc::Kind::returns:
        Javadoc::append(list, Javadoc::Returns());
        return Error::success();
    case Javadoc::Kind::param:
        Javadoc::append(list, Javadoc::Param());
        return Error::success();
    case Javadoc::Kind::tparam:
        Javadoc::append(list, Javadoc::TParam());
        return Error::success();
    default:
        return Error("invalid kind");
    }
}

template<class T>
Error
AnyNodeList::
NodesImpl<T>::
setStyle(
    Javadoc::Style style)
{
    if constexpr(std::derived_from<T, Javadoc::Text>)
    {
        if(list.back().kind != Javadoc::Kind::styled)
            return Error("style on wrong kind");
        auto& node = static_cast<Javadoc::StyledText&>(list.back());
        node.style = style;
        return Error::success();
    }
    else
    {
        return Error("style on wrong kind");
    }
}

template<class T>
Error
AnyNodeList::
NodesImpl<T>::
setString(
    llvm::StringRef string)
{
    if constexpr(std::derived_from<T, Javadoc::Text>)
    {
        auto& node = static_cast<Javadoc::Text&>(list.back());
        node.string = string.str();
        return Error::success();
    }
    else if constexpr(std::derived_from<T, Javadoc::Block>)
    {
        switch(list.back().kind)
        {
        case Javadoc::Kind::param:
        {
            auto& node = static_cast<Javadoc::Param&>(list.back());
            node.name = string.str();
            return Error::success();
        }
        case Javadoc::Kind::tparam:
        {
            auto& node = static_cast<Javadoc::TParam&>(list.back());
            node.name = string.str();
            return Error::success();
        }
        default:
            break;
        }
    }
#if 0
    // VFALCO This was for when the top level Javadoc
    // has separate lists for Param and TParam
    else if constexpr(std::derived_from<T, Javadoc::Param>)
    {
        auto& node = static_cast<Javadoc::Param&>(list.back());
        node.name = string.str();
        return Error::success();
    }
    else if constexpr(std::derived_from<T, Javadoc::TParam>)
    {
        auto& node = static_cast<Javadoc::TParam&>(list.back());
        node.name = string.str();
        return Error::success();
    }
#endif
    return Error("string on wrong kind");
}

template<class T>
Error
AnyNodeList::
NodesImpl<T>::
setAdmonish(
    Javadoc::Admonish admonish)
{
    if constexpr(std::derived_from<T, Javadoc::Block>)
    {
        if(list.back().kind != Javadoc::Kind::admonition)
            return Error("admonish on wrong kind");
        auto& node = static_cast<Javadoc::Admonition&>(list.back());
        node.style = admonish;
        return Error::success();
    }
    else
    {
        return Error("admonish on wrong kind");
    }
}

template<class T>
Error
AnyNodeList::
NodesImpl<T>::
setDirection(
    Javadoc::ParamDirection direction)
{
    if constexpr(std::derived_from<T, Javadoc::Block>)
    {
        if(list.back().kind != Javadoc::Kind::param)
            return Error("direction on wrong kind");
        auto& node = static_cast<Javadoc::Param&>(list.back());
        node.direction = direction;
        return Error::success();
    }
    else
    {
        return Error("direction on wrong kind");
    }
}

template<class T>
AnyListNodes
AnyNodeList::
NodesImpl<T>::
extractNodes()
{
    return list.extractNodes();
}

template<class T>
void
AnyNodeList::
NodesImpl<T>::
spliceBack(
    AnyListNodes&& nodes) noexcept
{
    if constexpr(std::derived_from<T, Javadoc::Block>)
    {
        list.back().children.spliceBack(std::move(nodes));
    }
    else
    {
    }
}

} // mrdox
} // clang

#endif
