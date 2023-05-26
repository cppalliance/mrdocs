//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_API_AST_ANYNODELIST_HPP
#define MRDOX_API_AST_ANYNODELIST_HPP

#include <mrdox/Platform.hpp>
#include <mrdox/Error.hpp>
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
        virtual llvm::Error appendChild(Javadoc::Kind kind) = 0;
        virtual llvm::Error setStyle(Javadoc::Style style) = 0;
        virtual llvm::Error setString(llvm::StringRef string) = 0;
        virtual llvm::Error setAdmonish(Javadoc::Admonish admonish) = 0;
        virtual llvm::Error setDirection(Javadoc::ParamDirection direction) = 0;
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
    llvm::Error setKind(Javadoc::Kind kind);

    template<class T>
    llvm::Error spliceInto(AnyList<T>& nodes);
    llvm::Error spliceIntoParent();

private:
    template<class T>
    struct NodesImpl : Nodes
    {
        AnyList<T> list;

        llvm::Error appendChild(Javadoc::Kind kind) override;
        llvm::Error setStyle(Javadoc::Style style) override;
        llvm::Error setString(llvm::StringRef string) override;
        llvm::Error setAdmonish(Javadoc::Admonish admonish) override;
        llvm::Error setDirection(Javadoc::ParamDirection direction) override;
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
        llvm::Error appendChild(Javadoc::Kind) override
        {
            return makeError("missing kind");
        }

        llvm::Error setStyle(Javadoc::Style) override
        {
            return makeError("missing kind");
        }

        llvm::Error setString(llvm::StringRef) override
        {
            return makeError("missing kind");
        }

        llvm::Error setAdmonish(Javadoc::Admonish) override
        {
            return makeError("missing kind");
        }

        llvm::Error setDirection(Javadoc::ParamDirection) override
        {
            return makeError("missing kind");
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
llvm::Error
AnyNodeList::
setKind(
    Javadoc::Kind kind)
{
    if(nodes_ != nullptr)
        return makeError("kind already set");
    switch(kind)
    {
    case Javadoc::Kind::block:
        nodes_ = new NodesImpl<Javadoc::Block>();
        break;
    case Javadoc::Kind::text:
        nodes_ = new NodesImpl<Javadoc::Text>();
        break;
    default:
        return makeError("wrong or unknown kind");
    }
    return llvm::Error::success();
}

template<class T>
llvm::Error
AnyNodeList::
spliceInto(
    AnyList<T>& nodes)
{
    if(nodes_ == nullptr)
        return makeError("splice without nodes");

    // splice `*nodes_` to the end of `nodes`
    nodes.spliceBack(nodes_->extractNodes());

    return llvm::Error::success();
}

inline
llvm::Error
AnyNodeList::
spliceIntoParent()
{
    if(prev_ == nullptr)
        return makeError("splice without parent");

    // splice `*nodes_` to the end of `*prev_->nodes_`
    prev_->nodes_->spliceBack(nodes_->extractNodes());

    return llvm::Error::success();
}

//------------------------------------------------

template<class T>
llvm::Error
AnyNodeList::
NodesImpl<T>::
appendChild(
    Javadoc::Kind kind)
{
    switch(kind)
    {
    case Javadoc::Kind::text:
        Javadoc::append(list, Javadoc::Text());
        return llvm::Error::success();
    case Javadoc::Kind::styled:
        Javadoc::append(list, Javadoc::StyledText());
        return llvm::Error::success();
    case Javadoc::Kind::paragraph:
        Javadoc::append(list, Javadoc::Paragraph());
        return llvm::Error::success();
    case Javadoc::Kind::brief:
        Javadoc::append(list, Javadoc::Brief());
        return llvm::Error::success();
    case Javadoc::Kind::admonition:
        Javadoc::append(list, Javadoc::Admonition());
        return llvm::Error::success();
    case Javadoc::Kind::code:
        Javadoc::append(list, Javadoc::Code());
        return llvm::Error::success();
    case Javadoc::Kind::returns:
        Javadoc::append(list, Javadoc::Returns());
        return llvm::Error::success();
    case Javadoc::Kind::param:
        Javadoc::append(list, Javadoc::Param());
        return llvm::Error::success();
    case Javadoc::Kind::tparam:
        Javadoc::append(list, Javadoc::TParam());
        return llvm::Error::success();
    default:
        return makeError("invalid kind");
    }
}

template<class T>
llvm::Error
AnyNodeList::
NodesImpl<T>::
setStyle(
    Javadoc::Style style)
{
    if constexpr(std::derived_from<T, Javadoc::Text>)
    {
        if(list.back().kind != Javadoc::Kind::styled)
            return makeError("style on wrong kind");
        auto& node = static_cast<Javadoc::StyledText&>(list.back());
        node.style = style;
        return llvm::Error::success();
    }
    else
    {
        return makeError("style on wrong kind");
    }
}

template<class T>
llvm::Error
AnyNodeList::
NodesImpl<T>::
setString(
    llvm::StringRef string)
{
    if constexpr(std::derived_from<T, Javadoc::Text>)
    {
        auto& node = static_cast<Javadoc::Text&>(list.back());
        node.string = string.str();
        return llvm::Error::success();
    }
    else if constexpr(std::derived_from<T, Javadoc::Block>)
    {
        switch(list.back().kind)
        {
        case Javadoc::Kind::param:
        {
            auto& node = static_cast<Javadoc::Param&>(list.back());
            node.name = string.str();
            return llvm::Error::success();
        }
        case Javadoc::Kind::tparam:
        {
            auto& node = static_cast<Javadoc::TParam&>(list.back());
            node.name = string.str();
            return llvm::Error::success();
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
        return llvm::Error::success();
    }
    else if constexpr(std::derived_from<T, Javadoc::TParam>)
    {
        auto& node = static_cast<Javadoc::TParam&>(list.back());
        node.name = string.str();
        return llvm::Error::success();
    }
#endif
    return makeError("string on wrong kind");
}

template<class T>
llvm::Error
AnyNodeList::
NodesImpl<T>::
setAdmonish(
    Javadoc::Admonish admonish)
{
    if constexpr(std::derived_from<T, Javadoc::Block>)
    {
        if(list.back().kind != Javadoc::Kind::admonition)
            return makeError("admonish on wrong kind");
        auto& node = static_cast<Javadoc::Admonition&>(list.back());
        node.style = admonish;
        return llvm::Error::success();
    }
    else
    {
        return makeError("admonish on wrong kind");
    }
}

template<class T>
llvm::Error
AnyNodeList::
NodesImpl<T>::
setDirection(
    Javadoc::ParamDirection direction)
{
    if constexpr(std::derived_from<T, Javadoc::Block>)
    {
        if(list.back().kind != Javadoc::Kind::param)
            return makeError("direction on wrong kind");
        auto& node = static_cast<Javadoc::Param&>(list.back());
        node.direction = direction;
        return llvm::Error::success();
    }
    else
    {
        return makeError("direction on wrong kind");
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
