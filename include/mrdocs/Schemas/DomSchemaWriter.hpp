//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_API_SCHEMAS_DOMSCHEMAWRITER_HPP
#define MRDOCS_API_SCHEMAS_DOMSCHEMAWRITER_HPP

#include <mrdocs/Dom/Array.hpp>
#include <mrdocs/Dom/Object.hpp>
#include <mrdocs/Dom/Value.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Schemas/DomDescriptions.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <mrdocs/Support/EnumToString.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

/** Schema writers for the MrDocs output formats.

    Houses the JSON-Schema writer for the DOM (Handlebars) objects
    and the RELAX NG Compact writer for the XML generator's output.
    The writers are driven by reflection over described types.
*/
namespace mrdocs::schema {

//------------------------------------------------
// Type -> JSON Schema dispatch
//------------------------------------------------

/** Return a JSON Schema `$ref` for a described struct type.
*/
template <typename T>
dom::Object refSchema()
{
    dom::Object schema;
    std::string ref = "#/$defs/";
    ref += readableTypeName<T>();
    schema.set("$ref", ref);
    return schema;
}

/** Return the JSON Schema for a single member type.

    Mirrors the type dispatch in MapReflectedType.hpp's `mapMember()`.
*/
template <typename T>
dom::Object
memberSchema()
{
    using Type = std::decay_t<T>;
    dom::Object schema;

    if constexpr (std::is_same_v<Type, bool>)
    {
        schema.set("type", "boolean");
    }
    else if constexpr (std::is_same_v<Type, std::string> ||
                       std::is_same_v<Type, dom::String>)
    {
        schema.set("type", "string");
    }
    else if constexpr (std::is_integral_v<Type>)
    {
        schema.set("type", "integer");
    }
    else if constexpr (std::is_same_v<Type, SymbolID>)
    {
        schema.set("type", "string");
        schema.set("description", "Base64-encoded symbol ID");
    }
    else if constexpr (std::is_base_of_v<ExprInfo, Type>)
    {
        // ExprInfo maps to its Written string in the DOM.
        schema.set("type", "string");
        schema.set("description", "C++ expression as written");
    }
    else if constexpr (describe::has_describe_enumerators<Type>::value)
    {
        schema.set("type", "string");
        dom::Array values;
        describe::for_each(
            describe::describe_enumerators<Type>{},
            [&](auto const& D) {
                values.push_back(toKebabCase(D.name));
            });
        schema.set("enum", std::move(values));
    }
    // Non-described enums with manual toString() or getOperatorName()
    // (described enums already handled above).
    else if constexpr (std::is_enum_v<Type>)
    {
        schema.set("type", "string");
    }
    // Non-described structs with custom ValueFrom that
    // serialize as strings (e.g. NoexceptInfo, ExplicitInfo).
    else if constexpr (dom::HasValueFromWithoutContext<Type>)
    {
        schema.set("type", "string");
    }
    else if constexpr (mrdocs::detail::is_optional_v<Type>)
    {
        // Unwrap optionals — the member is simply not in "required".
        schema = memberSchema<typename Type::value_type>();
    }
    else if constexpr (mrdocs::detail::is_vector_v<Type>)
    {
        schema.set("type", "array");
        schema.set("items", memberSchema<typename Type::value_type>());
    }
    else if constexpr (mrdocs::detail::is_polymorphic_v<Type>)
    {
        // Polymorphic types use a $ref to their oneOf definition.
        schema = refSchema<typename Type::value_type>();
    }
    else if constexpr (describe::has_describe_members<Type>::value)
    {
        // Described compound types reference $defs.
        schema = refSchema<Type>();
    }
    else
    {
        // Fallback: any JSON value.
        schema.set("description", "unknown type");
    }

    return schema;
}

//------------------------------------------------
// Helper: is the member always present?
//
// Mirrors shouldMapValue() in MapReflectedType.hpp:
// Optional, empty string, certain "None" enums,
// and empty ExprInfo are skipped at runtime, so
// those fields are NOT required in the schema.
//------------------------------------------------

/** True if `M` is always serialized, i.e. never skipped by
    `shouldMapValue()` in `MapReflectedType.hpp`.

    Members of such a type must appear in the JSON Schema's
    `required` array.
*/
template <typename M>
inline constexpr bool is_always_present_v =
    !mrdocs::detail::is_optional_v<M> &&
    !std::is_same_v<M, std::string> &&
    !std::is_same_v<M, ConstexprKind> &&
    !std::is_same_v<M, ReferenceKind> &&
    !std::is_same_v<M, StorageClassKind> &&
    !std::is_base_of_v<ExprInfo, M>;

//------------------------------------------------
// Description injection (from DomDescriptions.hpp)
//------------------------------------------------

/** Add the type-level description for `T` to `schema`, if one exists.
*/
template <typename T>
void
applyTypeDescription(dom::Object& schema)
{
    constexpr std::string_view name = readableTypeName<T>();
    if (auto const desc = findDomDescription(name); !desc.empty())
    {
        schema.set("description", std::string(desc));
    }
}

/** Build the schema for a member of `Owner` with the given DOM name,
    layering in any matching member-level description.
*/
template <typename Owner, typename M>
dom::Object
buildMemberSchema(std::string_view const domName)
{
    dom::Object schema = memberSchema<M>();
    constexpr std::string_view ownerName = readableTypeName<Owner>();
    if (auto const desc = findDomDescription(ownerName, domName);
        !desc.empty())
    {
        schema.set("description", std::string(desc));
    }
    return schema;
}

//------------------------------------------------
// Struct -> JSON Schema object
//------------------------------------------------

/** Add properties from base classes.
*/
template <typename T>
void
addBaseProperties(dom::Object& properties, dom::Array& required)
{
    if constexpr (describe::has_describe_bases<T>::value)
    {
        describe::for_each(
            describe::describe_bases<T>{},
            [&](auto const& descriptor)
            {
                using BaseType = typename std::decay_t<decltype(descriptor)>::type;
                addBaseProperties<BaseType>(properties, required);

                // Add the base's own members.
                if constexpr (describe::has_describe_members<BaseType>::value)
                {
                    describe::for_each(
                        describe::describe_members<BaseType>{},
                        [&](auto const& D) {
                            using M = std::decay_t<decltype(
                                std::declval<BaseType>().*D.pointer)>;
                            std::string domName =
                                mrdocs::detail::normalizeMemberName(D.name);
                            properties.set(
                                domName,
                                buildMemberSchema<BaseType, M>(domName));

                            if constexpr (is_always_present_v<M>)
                            {
                                required.push_back(domName);
                            }
                        });
                }
            });
    }
}

/** Build a JSON Schema "object" for a described type T.

    Includes properties from all base classes (recursively)
    plus T's own members.
*/
template <typename T>
dom::Object
objectSchema()
{
    dom::Object schema;
    schema.set("type", "object");
    applyTypeDescription<T>(schema);

    dom::Object properties;
    dom::Array required;

    addBaseProperties<T>(properties, required);

    // Add T's own members.
    if constexpr (describe::has_describe_members<T>::value)
    {
        describe::for_each(
            describe::describe_members<T>{},
            [&](auto const& D)
            {
                using M = std::decay_t<decltype(
                    std::declval<T>().*D.pointer)>;
                std::string domName =
                    mrdocs::detail::normalizeMemberName(D.name);
                properties.set(
                    domName, buildMemberSchema<T, M>(domName));

                if constexpr (is_always_present_v<M>)
                {
                    required.push_back(domName);
                }
            });
    }

    // Custom tag_invoke extensions for Symbol (see SymbolBase.hpp).
    // These fields are added by the Symbol tag_invoke overload
    // beyond what reflection provides. Descriptions live on the
    // base `Symbol` entry in DomDescriptions.hpp.
    if constexpr (std::is_base_of_v<Symbol, T>)
    {
        auto withDesc =
            [](dom::Object schema, std::string_view const member)
            {
                if (auto const desc =
                        findDomDescription("Symbol", member);
                    !desc.empty())
                {
                    schema.set("description", std::string(desc));
                }
                return schema;
            };

        dom::Object classSchema;
        classSchema.set("type", "string");
        classSchema.set("const", "symbol");
        properties.set("class", withDesc(std::move(classSchema), "class"));

        dom::Object boolSchema;
        boolSchema.set("type", "boolean");
        properties.set("isRegular",
            withDesc(boolSchema, "isRegular"));
        properties.set("isSeeBelow",
            withDesc(boolSchema, "isSeeBelow"));
        properties.set("isImplementationDefined",
            withDesc(boolSchema, "isImplementationDefined"));
        properties.set("isDependency",
            withDesc(boolSchema, "isDependency"));

        required.push_back("class");
        required.push_back("isRegular");
        required.push_back("isSeeBelow");
        required.push_back("isImplementationDefined");
        required.push_back("isDependency");
    }

    // $meta object (added by addMetaObject in MapReflectedType.hpp).
    {
        dom::Object metaSchema;
        metaSchema.set("type", "object");
        dom::Object metaProps;
        dom::Object typeStr;
        typeStr.set("type", "string");
        metaProps.set("type", std::move(typeStr));
        dom::Object basesArr;
        basesArr.set("type", "array");
        dom::Object strItems;
        strItems.set("type", "string");
        basesArr.set("items", std::move(strItems));
        metaProps.set("bases", std::move(basesArr));
        metaSchema.set("properties", std::move(metaProps));
        properties.set("$meta", std::move(metaSchema));
    }

    if (!properties.empty())
    {
        schema.set("properties", std::move(properties));
    }
    if (!required.empty())
    {
        schema.set("required", std::move(required));
    }

    return schema;
}

//------------------------------------------------
// Polymorphic oneOf schemas
//------------------------------------------------

/** Wrap a list of `$ref`s into a `oneOf` schema for `Base` and
    layer in any matching type-level description.
*/
template <typename Base>
dom::Object
makePolymorphicSchema(dom::Array oneOf)
{
    dom::Object schema;
    schema.set("oneOf", std::move(oneOf));
    applyTypeDescription<Base>(schema);
    return schema;
}

/** Return a JSON Schema `oneOf` union for a polymorphic base type.

    A full specialization is provided for each polymorphic family
    (`Type`, `Name`, `TParam`, `TArg`, `doc::Block`, `doc::Inline`);
    it enumerates the concrete subclasses via the family's `.inc`
    list and emits a `$ref` per subclass.

    @tparam Base The polymorphic base class.
    @return A schema object whose `oneOf` array references every
    concrete subclass of `Base`.
*/
template <typename Base>
dom::Object polymorphicSchema();

/** Specialization for the `Type` family (NamedType, PointerType, ...).

    @return `{ "oneOf": [ $ref for each TypeKind ] }`.
*/
template <>
inline dom::Object
polymorphicSchema<Type>()
{
    dom::Array oneOf;
    #define INFO(X) oneOf.push_back(refSchema<X##Type>());
    #include <mrdocs/Metadata/Type/TypeNodes.inc>
    return makePolymorphicSchema<Type>(std::move(oneOf));
}

/** Specialization for the `Name` family (IdentifierName, SpecializationName).

    @return `{ "oneOf": [ $ref for each NameKind ] }`.
*/
template <>
inline dom::Object
polymorphicSchema<Name>()
{
    dom::Array oneOf;
    #define INFO(X) oneOf.push_back(refSchema<X##Name>());
    #include <mrdocs/Metadata/Name/NameNodes.inc>
    return makePolymorphicSchema<Name>(std::move(oneOf));
}

/** Specialization for the `TParam` family (TypeTParam, ConstantTParam,
    TemplateTParam).

    @return `{ "oneOf": [ $ref for each TParamKind ] }`.
*/
template <>
inline dom::Object
polymorphicSchema<TParam>()
{
    dom::Array oneOf;
    #define INFO(X) oneOf.push_back(refSchema<X##TParam>());
    #include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>
    return makePolymorphicSchema<TParam>(std::move(oneOf));
}

/** Specialization for the `TArg` family (TypeTArg, ConstantTArg,
    TemplateTArg).

    @return `{ "oneOf": [ $ref for each TArgKind ] }`.
*/
template <>
inline dom::Object
polymorphicSchema<TArg>()
{
    dom::Array oneOf;
    #define INFO(X) oneOf.push_back(refSchema<X##TArg>());
    #include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>
    return makePolymorphicSchema<TArg>(std::move(oneOf));
}

/** Specialization for the `doc::Block` family (paragraphs, lists,
    headings, and command blocks like brief/param/returns).

    @return `{ "oneOf": [ $ref for each BlockKind ] }`.
*/
template <>
inline dom::Object
polymorphicSchema<doc::Block>()
{
    dom::Array oneOf;
    #define INFO(X) oneOf.push_back(refSchema<doc::X##Block>());
    #include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
    return makePolymorphicSchema<doc::Block>(std::move(oneOf));
}

/** Specialization for the `doc::Inline` family (text, emphasis, code,
    references, etc.).

    @return `{ "oneOf": [ $ref for each InlineKind ] }`.
*/
template <>
inline dom::Object
polymorphicSchema<doc::Inline>()
{
    dom::Array oneOf;
    #define INFO(X) oneOf.push_back(refSchema<doc::X##Inline>());
    #include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>
    return makePolymorphicSchema<doc::Inline>(std::move(oneOf));
}

//------------------------------------------------
// Register all type definitions
//------------------------------------------------

/** Register a single described type in $defs.
*/
template <typename T>
void
registerDef(dom::Object& defs)
{
    constexpr std::string_view name = readableTypeName<T>();
    defs.set(std::string(name), objectSchema<T>());
}

/** Register all described types in $defs.
*/
inline void
registerAllDefs(dom::Object& defs)
{
    // Symbol types
    #define INFO(X) registerDef<X##Symbol>(defs);
    #include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

    // Type variants
    #define INFO(X) registerDef<X##Type>(defs);
    #include <mrdocs/Metadata/Type/TypeNodes.inc>

    // Name variants
    #define INFO(X) registerDef<X##Name>(defs);
    #include <mrdocs/Metadata/Name/NameNodes.inc>

    // TParam variants
    #define INFO(X) registerDef<X##TParam>(defs);
    #include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>

    // TArg variants
    #define INFO(X) registerDef<X##TArg>(defs);
    #include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>

    // Doc block variants
    #define INFO(X) registerDef<doc::X##Block>(defs);
    #include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

    // Doc inline variants
    #define INFO(X) registerDef<doc::X##Inline>(defs);
    #include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

    // Polymorphic union types
    defs.set("Type", polymorphicSchema<Type>());
    defs.set("Name", polymorphicSchema<Name>());
    defs.set("TParam", polymorphicSchema<TParam>());
    defs.set("TArg", polymorphicSchema<TArg>());
    defs.set("Block", polymorphicSchema<doc::Block>());
    defs.set("Inline", polymorphicSchema<doc::Inline>());

    // Supporting types
    registerDef<Location>(defs);
    registerDef<Param>(defs);
    registerDef<TemplateInfo>(defs);
    registerDef<SourceInfo>(defs);
    registerDef<BaseInfo>(defs);
    registerDef<FriendInfo>(defs);
}

//------------------------------------------------
// Top-level schema
//------------------------------------------------

/** Build the complete JSON Schema describing the DOM.

    The returned `dom::Value` is a JSON Schema (draft 2020-12)
    document that describes the structure of every Handlebars
    DOM object MrDocs exposes to its templates.
*/
inline dom::Value
buildDomJsonSchema()
{
    dom::Object schema;
    schema.set("$schema", "https://json-schema.org/draft/2020-12/schema");
    schema.set("title", "MrDocs DOM Schema");
    schema.set("description",
        "Schema for the DOM objects available to Handlebars "
        "templates in the MrDocs documentation generator.");

    // Symbol is the entry point — a oneOf over all symbol kinds.
    dom::Array symbolOneOf;
    #define INFO(X) symbolOneOf.push_back(refSchema<X##Symbol>());
    #include <mrdocs/Metadata/Symbol/SymbolNodes.inc>
    schema.set("oneOf", std::move(symbolOneOf));

    dom::Object defs;
    registerAllDefs(defs);
    schema.set("$defs", std::move(defs));

    return schema;
}

} // mrdocs::schema

#endif // MRDOCS_API_SCHEMAS_DOMSCHEMAWRITER_HPP
