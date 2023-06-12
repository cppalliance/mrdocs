//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_TOOL_AST_ANYNODELIST_HPP
#define MRDOX_TOOL_AST_ANYNODELIST_HPP

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
        virtual Error appendChild(doc::Kind kind) = 0;
        virtual Error setStyle(doc::Style style) = 0;
        virtual Error setString(llvm::StringRef string) = 0;
        virtual Error setAdmonish(doc::Admonish admonish) = 0;
        virtual Error setDirection(doc::ParamDirection direction) = 0;
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
    Error setKind(doc::Kind kind);

    template<class T>
    Error spliceInto(AnyList<T>& nodes);
    Error spliceIntoParent();

private:
    template<class T>
    struct NodesImpl : Nodes
    {
        AnyList<T> list;

        Error appendChild(doc::Kind kind) override;
        Error setStyle(doc::Style style) override;
        Error setString(llvm::StringRef string) override;
        Error setAdmonish(doc::Admonish admonish) override;
        Error setDirection(doc::ParamDirection direction) override;
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
        Error appendChild(doc::Kind) override
        {
            return Error("kind is missing");
        }

        Error setStyle(doc::Style) override
        {
            return Error("kind is missing");
        }

        Error setString(llvm::StringRef) override
        {
            return Error("kind is missing");
        }

        Error setAdmonish(doc::Admonish) override
        {
            return Error("kind is missing");
        }

        Error setDirection(doc::ParamDirection) override
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
    doc::Kind kind)
{
    if(nodes_ != nullptr)
        return Error("kind already set");
    switch(kind)
    {
    case doc::Kind::block:
        nodes_ = new NodesImpl<doc::Block>();
        break;
    case doc::Kind::text:
        nodes_ = new NodesImpl<doc::Text>();
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
    doc::Kind kind)
{
    switch(kind)
    {
    case doc::Kind::text:
        Javadoc::append(list, doc::Text());
        return Error::success();
    case doc::Kind::styled:
        Javadoc::append(list, doc::StyledText());
        return Error::success();
    case doc::Kind::paragraph:
        Javadoc::append(list, doc::Paragraph());
        return Error::success();
    case doc::Kind::brief:
        Javadoc::append(list, doc::Brief());
        return Error::success();
    case doc::Kind::admonition:
        Javadoc::append(list, doc::Admonition());
        return Error::success();
    case doc::Kind::code:
        Javadoc::append(list, doc::Code());
        return Error::success();
    case doc::Kind::returns:
        Javadoc::append(list, doc::Returns());
        return Error::success();
    case doc::Kind::param:
        Javadoc::append(list, doc::Param());
        return Error::success();
    case doc::Kind::tparam:
        Javadoc::append(list, doc::TParam());
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
    doc::Style style)
{
    if constexpr(std::derived_from<T, doc::Text>)
    {
        if(list.back().kind != doc::Kind::styled)
            return Error("style on wrong kind");
        auto& node = static_cast<doc::StyledText&>(list.back());
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
    if constexpr(std::derived_from<T, doc::Text>)
    {
        auto& node = static_cast<doc::Text&>(list.back());
        node.string = string.str();
        return Error::success();
    }
    else if constexpr(std::derived_from<T, doc::Block>)
    {
        switch(list.back().kind)
        {
        case doc::Kind::param:
        {
            auto& node = static_cast<doc::Param&>(list.back());
            node.name = string.str();
            return Error::success();
        }
        case doc::Kind::tparam:
        {
            auto& node = static_cast<doc::TParam&>(list.back());
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
    else if constexpr(std::derived_from<T, doc::Param>)
    {
        auto& node = static_cast<doc::Param&>(list.back());
        node.name = string.str();
        return Error::success();
    }
    else if constexpr(std::derived_from<T, doc::TParam>)
    {
        auto& node = static_cast<doc::TParam&>(list.back());
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
    doc::Admonish admonish)
{
    if constexpr(std::derived_from<T, doc::Block>)
    {
        if(list.back().kind != doc::Kind::admonition)
            return Error("admonish on wrong kind");
        auto& node = static_cast<doc::Admonition&>(list.back());
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
    doc::ParamDirection direction)
{
    if constexpr(std::derived_from<T, doc::Block>)
    {
        if(list.back().kind != doc::Kind::param)
            return Error("direction on wrong kind");
        auto& node = static_cast<doc::Param&>(list.back());
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
    if constexpr(std::derived_from<T, doc::Block>)
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
