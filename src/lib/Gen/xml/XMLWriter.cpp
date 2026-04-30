//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
// Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "XMLWriter.hpp"
#include <mrdocs/Metadata/Attributes.hpp>
#include <mrdocs/Metadata/DocComment.hpp>
#include <mrdocs/Metadata/Expression.hpp>
#include <mrdocs/Metadata/Specifiers.hpp>
#include <mrdocs/Metadata/Template.hpp>
#include <mrdocs/Metadata/Type.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>

namespace mrdocs::xml {

//------------------------------------------------
// Type traits
//------------------------------------------------

namespace {

// A token type has no internal whitespace when represented as a string.
template <typename T>
constexpr bool is_token =
    !describe::has_describe_members<T>::value &&
    (std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_same_v<T, SymbolID>);

// A described object collapses to plain text when it has a single string member
template <typename T>
constexpr bool is_single_text_object =
    describe::has_describe_members<T>::value &&
    describe::describedMemberCount<T>() == 1 &&
    describe::describedMembersAllText<T>();

// A type represented as a string
template <typename T>
constexpr bool is_text =
    (!describe::has_describe_members<T>::value &&
     std::convertible_to<T, std::string_view>) ||
    is_single_text_object<T>;

// Types represented as leaf nodes in the XML tree
template <typename T>
constexpr bool is_leaf_text = is_token<T> || is_text<T>;

// A range whose members are not reflected
template <typename T>
concept unreflected_range =
    std::ranges::range<T> &&
    !is_leaf_text<T> &&
    !describe::has_describe_members<T>::value;

// A runtime-polymorphic base reached through its `visit` overload (Type,
// Attribute, TParam, TArg, Name, Symbol, doc::Block, doc::Inline). visit
// dispatches to the concrete kind, which is then written under its own type tag.
template <typename T>
concept visitable = requires (T const& t) { visit(t, [](auto const&) {}); };

// Whether `value` is omitted from the XML output. A field with such a value
// produces no element, and an object all of whose members are omitted produces
// no children, so it is omitted too. Each type has its own test: a boolean is
// omitted when false, a string when empty, a list when every entry is omitted,
// an object when every member is omitted, and so on. The test is conservative:
// anything not provably omitted (a live variant, an integer, a plain enum)
// counts as present, so at worst a genuinely empty object is kept -- a non-empty
// one is never dropped.
template <typename T>
bool
isOmittedFromXML(T const& value)
{
    using Type = std::decay_t<T>;
    if constexpr (std::is_same_v<Type, bool>)
        return !value;
    else if constexpr (describe::has_undefined_enumerator<Type>)
        return value == describe::undefined_enumerator<Type>;
    else if constexpr (is_specialization_of_v<Type, Optional>)
        return !value || isOmittedFromXML(*value);
    else if constexpr (is_specialization_of_v<Type, Polymorphic>)
        return value.valueless_after_move();
    else if constexpr (unreflected_range<Type>)
        return std::ranges::all_of(
            value, [](auto const& e) { return isOmittedFromXML(e); });
    else if constexpr (std::is_same_v<Type, SymbolID>)
        return !value;
    else if constexpr (std::convertible_to<Type, std::string_view>)
        return std::string_view(value).empty();
    else if constexpr (describe::has_describe_members<Type>::value)
    {
        bool omitted = true;
        describe::for_each_member<Type>(
            [&](auto d) { omitted = omitted && isOmittedFromXML(value.*d.pointer); });
        return omitted;
    }
    else
        return false;
}

// Strip from `name` any trailing repetition of a base class name. For instance,
// "FunctionSymbol" becomes "Function" because the base class is "Symbol".
template <typename T>
void
stripBaseName(std::string& name)
{
    if constexpr (describe::has_describe_bases<T>::value)
    {
        describe::for_each(describe::describe_bases<T>{}, [&](auto D) {
            using Base = typename std::decay_t<decltype(D)>::type;
            std::string_view const base = readableTypeName<Base>();
            if (name.size() > base.size() &&
                std::string_view(name).substr(name.size() - base.size()) == base)
            {
                name.erase(name.size() - base.size());
            }
            stripBaseName<Base>(name);
        });
    }
}

// The default element tag for an object value
// It's defined as the camel case version of the type name with any base
// class name stripped.
template <typename T>
std::string typeTag()
{
    std::string name(readableTypeName<T>());
    stripBaseName<T>(name);
    return toCamelCase(name);
}

} // unnamed namespace

XMLWriter::XMLWriter(llvm::raw_ostream& os, Corpus const& corpus) noexcept
    : tags_(os), os_(os), corpus_(corpus) {}

Expected<void>
XMLWriter::build()
{
    os_ << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<mrdocs xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
           "       xsi:noNamespaceSchemaLocation=\"https://github.com/cppalliance/mrdocs/raw/develop/mrdocs.rnc\">\n";
    (*this)(corpus_.globalNamespace());
    // Macros are corpus-level: they are not in any C++ scope,
    // so they appear as siblings of the global namespace rather
    // than under it.
    for (MacroSymbol const* m : corpus_.macros())
    {
        (*this)(*m);
    }
    os_ << "</mrdocs>\n";
    return {};
}

template <std::derived_from<Symbol> SymbolTy>
void
XMLWriter::operator()(SymbolTy const& I)
{
    MRDOCS_CHECK_OR(I.Extraction != ExtractionMode::Dependency);
    // Symbols are a flat list of typed elements at the top level; the tree
    // structure is recovered from the SymbolID references in their fields.
    writeObject(typeTag<SymbolTy>(), I);
    corpus_.traverse(I, *this);
}

#define INFO(Type) template void XMLWriter::operator()<Type##Symbol>(Type##Symbol const&);
#include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

template <typename T>
void
XMLWriter::writeObject(std::string const& tag, T const& value)
{
    // An object all of whose members are omitted has no children; writing it as
    // an empty `<tag></tag>` is meaningless, so omit the whole element.
    if (isOmittedFromXML(value))
    {
        return;
    }
    tags_.open(tag);
    writeObjectBody(value);
    tags_.close(tag);
}

template <typename T>
void
XMLWriter::writeObjectBody(T const& obj)
{
    describe::for_each_member<T>([&](auto D) {
        writeObjectField(toCamelCase(D.name), obj.*D.pointer);
    });
}

template <typename T>
void
XMLWriter::writeObjectField(std::string_view tag, T const& value)
{
    using Type = std::decay_t<T>;
    // A boolean is written as presence: `true` is an empty element `<tag/>`,
    // `false` is omitted entirely (an absent element reads as false). Almost
    // every boolean in MrDocs metadata is false, so this keeps the output
    // compact.
    if constexpr (std::is_same_v<Type, bool>)
    {
        if (value)
        {
            tags_.indent() << '<' << tag << "/>\n";
        }
        return;
    }
    // An enum with a designated undefined state is absent when it holds that
    // value: treat it like an empty optional and omit the field entirely.
    if constexpr (describe::has_undefined_enumerator<Type>)
    {
        if (value == describe::undefined_enumerator<Type>)
            return;
    }
    // Optional: emit only when it holds a value.
    if constexpr (is_specialization_of_v<Type, Optional>)
    {
        if (value)
        {
            writeObjectField(tag, *value);
        }
    }
    // Variant in a field: wrap twice
    else if constexpr (is_specialization_of_v<Type, Polymorphic>)
    {
        if (!value.valueless_after_move())
        {
            tags_.open(std::string(tag));
            writeValue(*value);
            tags_.close(std::string(tag));
        }
    }
    // Lists
    else if constexpr (unreflected_range<Type>)
    {
        if (value.empty())
            return;
        using Elem = std::decay_t<std::ranges::range_value_t<Type>>;
        if constexpr (is_token<Elem>)
        {
            // List of token scalars: values space-separated.
            tags_.indent() << '<' << tag << '>';
            char const* sep = "";
            for (auto const& item : value)
            {
                os_ << sep;
                writeScalar(os_, item);
                sep = " ";
            }
            os_ << "</" << tag << ">\n";
        }
        else
        {
            // Object lists: typed objects entry
            tags_.open(std::string(tag));
            for (auto const& item: value)
            {
                writeValue(item);
            }
            tags_.close(std::string(tag));
        }
    }
    // Leaf nodes
    else if constexpr (is_leaf_text<Type>)
    {
        if constexpr (std::is_same_v<Type, SymbolID>)
        { if (!value) return; }
        std::string text;
        {
            llvm::raw_string_ostream os(text);
            writeScalar(os, value);
        }
        if (text.empty())
        {
            return;
        }
        tags_.indent() << '<' << tag << '>' << text << "</" << tag << ">\n";
    }
    // rule 4 - objects: a child element per field.
    else if constexpr (describe::has_describe_members<Type>::value)
    {
        // An expression with no written form is absent, so an empty
        // ConstantExprInfo (e.g. a non-bitfield's width) is omitted. This is
        // the same rule the DOM applies in `shouldMapValue`, keeping the
        // generators in sync.
        if constexpr (std::is_base_of_v<ExprInfo, Type>)
        {
            if (value.Written.empty())
                return;
        }
        writeObject(std::string(tag), value);
    }
}

// Emit the text content of a scalar (rule 5) to `os`. No tags.
template <typename T>
void
XMLWriter::writeScalar(llvm::raw_ostream& os, T const& value)
{
    using Type = std::decay_t<T>;
    if constexpr (std::is_same_v<Type, bool>)
    {
        os << (value ? "true" : "false");
    }
    else if constexpr (describe::has_describe_enumerators<Type>::value)
    {
        // Described enum: its kebab name via the generic toString. Qualified
        // because the local toString(SymbolID) would hide it, and enums in
        // mrdocs::doc cannot reach it through ADL from here.
        os << xmlEscape(mrdocs::toString(value));
    }
    else if constexpr (std::is_enum_v<Type> || std::is_same_v<Type, SymbolID>)
    {
        // A base64 SymbolID, or an undescribed enum with its own toString.
        os << xmlEscape(toString(value));
    }
    else if constexpr (is_single_text_object<Type>)
    {
        describe::for_each_member<T>([&](auto d) {
            writeScalar(os, value.*d.pointer);
        });
    }
    else if constexpr (std::convertible_to<Type, std::string_view>)
    {
        os << xmlEscape(std::string_view(value));
    }
    else // integers
    {
        os << value;
    }
}

template <typename T>
void
XMLWriter::writeValue(T const& value)
{
    using Type = std::decay_t<T>;

    // A variant resolves to the concrete type it holds (rule 8).
    if constexpr (is_specialization_of_v<Type, Polymorphic>)
    {
        if (!value.valueless_after_move())
        {
            writeValue(*value);
        }
    }
    // Runtime-polymorphic bases dispatch on their Kind via `visit`.
    else if constexpr (visitable<Type>)
    {
        visit(value, [&]<class C>(C const& concrete) {
            writeObject(typeTag<std::decay_t<C>>(), concrete);
        });
    }
    // A whitespace-capable scalar list entry (rule 6b): a <string> child.
    else if constexpr (is_leaf_text<Type>)
    {
        tags_.indent() << "<string>";
        writeScalar(os_, value);
        os_ << "</string>\n";
    }
    // A plain reflected object list entry (rule 7): tag is its type name.
    else if constexpr (describe::has_describe_members<Type>::value)
    {
        writeObject(typeTag<Type>(), value);
    }
}

} // namespace mrdocs::xml
