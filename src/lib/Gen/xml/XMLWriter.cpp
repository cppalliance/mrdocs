//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "XMLWriter.hpp"
#include <mrdocs/Metadata/Attributes.hpp>
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/EnumToString.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>
#include <string>
#include <string_view>
#include <type_traits>

namespace mrdocs::xml {

//------------------------------------------------
// Type traits
//------------------------------------------------

namespace {

template <typename T> struct is_optional : std::false_type {};
template <typename T> struct is_optional<Optional<T>> : std::true_type {};

template <typename T> struct is_vector : std::false_type {};
template <typename T, typename A> struct is_vector<std::vector<T, A>> : std::true_type {};

template <typename T> struct is_polymorphic : std::false_type {};
template <typename T> struct is_polymorphic<Polymorphic<T>> : std::true_type {};

std::string
removeSuffix(std::string_view s, std::string_view suffix)
{
    return s.size() >= suffix.size() &&
           s.substr(s.size() - suffix.size()) == suffix
        ? std::string(s.substr(0, s.size() - suffix.size()))
        : std::string(s);
}

template <typename T>
std::string tagName()
{
    std::string name(readableTypeName<T>());
    for (std::string_view const suffix : {"Symbol", "Info", "TypeInfo", "TParam", "TArg", "Block", "Inline"})
    {
        name = removeSuffix(name, suffix);
    }
    return toKebabCase(name);
}

} // unnamed namespace

//------------------------------------------------
// XMLWriter
//------------------------------------------------

XMLWriter::XMLWriter(llvm::raw_ostream& os, Corpus const& corpus) noexcept
    : tags_(os), os_(os), corpus_(corpus) {}

Expected<void>
XMLWriter::build()
{
    os_ << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<mrdocs xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
           "       xsi:noNamespaceSchemaLocation=\"https://github.com/cppalliance/mrdocs/raw/develop/mrdocs.rnc\">\n";
    (*this)(corpus_.globalNamespace());
    os_ << "</mrdocs>\n";
    return {};
}

//------------------------------------------------
// Symbol visitor
//------------------------------------------------

template <std::derived_from<Symbol> SymbolTy>
void
XMLWriter::operator()(SymbolTy const& I)
{
    if (I.Extraction == ExtractionMode::Dependency)
        return;

    tags_.open(tagName<SymbolTy>());
    write(I);
    tags_.close(tagName<SymbolTy>());

    corpus_.traverse(I, *this);
}

//------------------------------------------------
// Core recursive writer
//------------------------------------------------

template <typename T>
void
XMLWriter::write(T const& value)
{
    using Type = std::decay_t<T>;

    // Primitives: write as text content
    if constexpr (std::is_same_v<Type, std::string> ||
                  std::is_same_v<Type, dom::String> ||
                  std::is_same_v<Type, llvm::StringRef>)
    {
        os_ << xmlEscape(value);
    }
    else if constexpr (std::is_integral_v<Type>)
    {
        os_ << value;
    }
    else if constexpr (std::is_same_v<Type, SymbolID>)
    {
        os_ << toBase64Str(value);
    }
    else if constexpr (describe::has_describe_enumerators<Type>::value)
    {
        os_ << toString(value);
    }
    // Wrappers: unwrap.
    else if constexpr (is_optional<Type>::value)
    {
        if (value)
        {
            write(*value);
        }
    }
    else if constexpr (is_polymorphic<Type>::value)
    {
        if (!value.valueless_after_move())
        {
            writePolymorphic(*value);
        }
    }
    // Containers: iterate.
    else if constexpr (is_vector<Type>::value)
    {
        for (auto const& item : value)
        {
            writeElement("item", item);
        }
    }
    // Described types: recurse.
    else // if constexpr (describe::has_describe_members<Type>::value)
    {
        writeMembers(value);
    }
}

template <typename T>
void
XMLWriter::writeElement(std::string_view tag, T const& value)
{
    using Type = std::decay_t<T>;

    // Skip empty values.
    if constexpr (std::is_same_v<Type, std::string> || std::is_same_v<Type, dom::String>)
        { if (value.empty()) return; }
    else if constexpr (std::is_same_v<Type, bool>)
        { if (!value) return; }
    else if constexpr (std::is_same_v<Type, SymbolID>)
        { if (!value) return; }
    else if constexpr (is_optional<Type>::value)
        { if (!value) return; }
    else if constexpr (is_vector<Type>::value)
        { if (value.empty()) return; }
    else if constexpr (std::is_base_of_v<ExprInfo, Type>)
        { if (value.Written.empty()) return; }

    // Primitives inline, compounds wrapped.
    if constexpr (std::is_base_of_v<ExprInfo, Type>)
    {
        // ExprInfo and ConstantExprInfo: write the Written string.
        tags_.indent() << "<" << tag << ">";
        os_ << xmlEscape(value.Written);
        os_ << "</" << tag << ">\n";
    }
    else if constexpr (std::is_same_v<Type, bool>)
    {
        // A false `bool` was skipped above. A true `bool` carries no
        // text body: the element's presence alone encodes the value,
        // so emit an empty element.
        tags_.indent() << '<' << tag << "/>\n";
    }
    else if constexpr (std::is_same_v<Type, std::string> ||
                  std::is_same_v<Type, dom::String> ||
                  std::is_integral_v<Type> ||
                  std::is_same_v<Type, SymbolID> ||
                  describe::has_describe_enumerators<Type>::value)
    {
        tags_.indent() << "<" << tag << ">";
        write(value);
        os_ << "</" << tag << ">\n";
    }
    else if constexpr (is_optional<Type>::value)
    {
        writeElement(tag, *value);
    }
    else if constexpr (is_polymorphic<Type>::value)
    {
        if (!value.valueless_after_move())
        {
            writePolymorphic(*value);
        }
    }
    else if constexpr (is_vector<Type>::value)
    {
        for (auto const& item : value)
            writeElement(tag, item);
    }
    else if constexpr (describe::has_describe_members<Type>::value)
    {
        tags_.open(tagName<Type>());
        writeMembers(value);
        tags_.close(tagName<Type>());
    }
}

template <typename T>
void
XMLWriter::writeMembers(T const& obj)
{
    // Write base class members first.
    if constexpr (describe::has_describe_bases<T>::value)
    {
        describe::for_each(
            describe::describe_bases<T>{},
            [&](auto D) {
                using Base = typename std::decay_t<decltype(D)>::type;
                if constexpr (describe::has_describe_members<Base>::value)
                {
                    writeMembers(static_cast<Base const&>(obj));
                }
            });
    }

    // Write direct members.
    if constexpr (describe::has_describe_members<T>::value)
    {
        describe::for_each(
            describe::describe_members<T>{},
            [&](auto D) {
                writeElement(toKebabCase(D.name), obj.*D.pointer);
            });
    }
}

//------------------------------------------------
// Polymorphic dispatch
//------------------------------------------------

template <typename T>
void
XMLWriter::writePolymorphic(T const& value)
{
    if constexpr (std::is_base_of_v<Type, T>)
    {
        switch (value.Kind)
        {
        #define INFO(Name) case TypeKind::Name: \
            tags_.open(toKebabCase(#Name)); \
            writeMembers(static_cast<Name##Type const&>(value)); \
            tags_.close(toKebabCase(#Name)); \
            break;
#include <mrdocs/Metadata/Type/TypeNodes.inc>
        default: MRDOCS_UNREACHABLE();
        }
    }
    else if constexpr (std::is_base_of_v<::mrdocs::Attribute, T>)
    {
        switch (value.Kind)
        {
        #define INFO(Name) case ::mrdocs::AttributeKind::Name: \
            tags_.open(toKebabCase(#Name) + "-attribute"); \
            writeMembers(value.as##Name()); \
            tags_.close(toKebabCase(#Name) + "-attribute"); \
            break;
#include <mrdocs/Metadata/Attribute/AttributeNodes.inc>
        default: MRDOCS_UNREACHABLE();
        }
    }
    else if constexpr (std::is_base_of_v<TParam, T>)
    {
        switch (value.Kind)
        {
        #define INFO(Name) case TParamKind::Name: \
            tags_.open(toKebabCase(#Name) + "-tparam"); \
            writeMembers(static_cast<Name##TParam const&>(value)); \
            tags_.close(toKebabCase(#Name) + "-tparam"); \
            break;
#include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>
        default: MRDOCS_UNREACHABLE();
        }
    }
    else if constexpr (std::is_base_of_v<TArg, T>)
    {
        switch (value.Kind)
        {
        #define INFO(Name) case TArgKind::Name: \
            tags_.open(toKebabCase(#Name) + "-targ"); \
            writeMembers(static_cast<Name##TArg const&>(value)); \
            tags_.close(toKebabCase(#Name) + "-targ"); \
            break;
#include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>
        default: MRDOCS_UNREACHABLE();
        }
    }
    else if constexpr (std::is_base_of_v<doc::Block, T>)
    {
        switch (value.Kind)
        {
        #define INFO(Name) case doc::BlockKind::Name: \
            tags_.open(toKebabCase(#Name)); \
            writeMembers(value.as##Name()); \
            tags_.close(toKebabCase(#Name)); \
            break;
#include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
        default: MRDOCS_UNREACHABLE();
        }
    }
    else if constexpr (std::is_base_of_v<doc::Inline, T>)
    {
        switch (value.Kind)
        {
        #define INFO(Name) case doc::InlineKind::Name: \
            tags_.open(toKebabCase(#Name)); \
            writeMembers(value.as##Name()); \
            tags_.close(toKebabCase(#Name)); \
            break;
#include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>
        default: MRDOCS_UNREACHABLE();
        }
    }
    else if constexpr (describe::has_describe_members<T>::value)
    {
        tags_.open(toKebabCase(readableTypeName<T>()));
        writeMembers(value);
        tags_.close(toKebabCase(readableTypeName<T>()));
    }
}

//------------------------------------------------
// Explicit instantiations
//------------------------------------------------

#define INSTANTIATE(Type) \
    template void XMLWriter::operator()<Type>(Type const&);

INSTANTIATE(NamespaceSymbol)
INSTANTIATE(EnumSymbol)
INSTANTIATE(EnumConstantSymbol)
INSTANTIATE(FunctionSymbol)
INSTANTIATE(OverloadsSymbol)
INSTANTIATE(GuideSymbol)
INSTANTIATE(ConceptSymbol)
INSTANTIATE(NamespaceAliasSymbol)
INSTANTIATE(UsingSymbol)
INSTANTIATE(RecordSymbol)
INSTANTIATE(TypedefSymbol)
INSTANTIATE(VariableSymbol)
#undef INSTANTIATE

} // namespace mrdocs::xml
