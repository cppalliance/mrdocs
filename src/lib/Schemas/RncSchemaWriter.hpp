//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SCHEMAS_RNCSCHEMAWRITER_HPP
#define MRDOCS_LIB_SCHEMAS_RNCSCHEMAWRITER_HPP

#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <mrdocs/Support/EnumToString.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>
#include <mrdocs/Support/String.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace mrdocs::schema {

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------

/** Implementation details for the RNC schema writer.
*/
namespace rnc_detail {

/** Remove `suffix` from the end of `s`, if present.

    @param s The input string.
    @param suffix The suffix to strip.
    @return `s` with the trailing `suffix` removed, or an
    unchanged copy of `s` if the suffix is not present.
*/
inline std::string
removeSuffix(std::string_view s, std::string_view suffix)
{
    // Strict greater-than guards against emptying the result when
    // a base struct (e.g. the bare `Type`) shares its name with a
    // listed suffix.
    return s.size() > suffix.size() &&
           s.substr(s.size() - suffix.size()) == suffix
        ? std::string(s.substr(0, s.size() - suffix.size()))
        : std::string(s);
}

/** XML element tag name for a described type (mirrors XMLWriter).

    @tparam T A described type.
    @return The kebab-case tag name produced by stripping well-known
    suffixes (Symbol, Info, TypeInfo, Type, TParam, TArg, Block,
    Inline) from the type's unqualified name. TParam-derived types
    are suffixed with "-tparam" and TArg-derived types with "-targ"
    to mirror XMLWriter's polymorphic dispatch (e.g. TypeTParam
    becomes "type-tparam"). The "Name" suffix is intentionally not
    stripped: the base struct `Name` itself stays "name" and concrete
    name types like `IdentifierName` stay "identifier-name", matching
    XMLWriter's writePolymorphic which uses the base/concrete type
    name directly.
*/
template <typename T>
std::string tagName()
{
    std::string raw(readableTypeName<T>());
    // Detect TParam/TArg derivation by suffix on the raw type name
    // (avoids needing TParam/TArg base types in template-instantiation
    // contexts where they may be incomplete).
    bool const isTParamSub =
        raw.size() > 6 && raw.substr(raw.size() - 6) == "TParam";
    bool const isTArgSub =
        raw.size() > 4 && raw.substr(raw.size() - 4) == "TArg";

    std::string name = raw;
    for (std::string_view const suffix :
         {"Symbol", "Info", "TypeInfo", "Type",
          "TParam", "TArg", "Block", "Inline"})
    {
        name = removeSuffix(name, suffix);
    }
    std::string out = toKebabCase(name);
    if (isTParamSub)
    {
        out += "-tparam";
    }
    else if (isTArgSub)
    {
        out += "-targ";
    }
    return out;
}

/** RNC pattern name (PascalCase, used for definitions).

    @tparam T A described type.
    @return The unqualified type name of `T`, used as the RNC
    pattern identifier on the left-hand side of a grammar rule.
*/
template <typename T>
std::string patternName()
{
    return std::string(readableTypeName<T>());
}

// ---------------------------------------------------------------
// RNC content type for a C++ member type
// ---------------------------------------------------------------

/** RNC pattern reference for the content of a member.

    @tparam T The C++ member type.
    @return The RNC expression describing the allowed content:
    a primitive (`text`, `Bool`, `SymbolID`) for scalar/string-like
    types, the pattern name of a described struct, or the
    appropriate union pattern (`AnyType`, `AnyTParam`, `AnyTArg`,
    `BlockNode`, `InlineNode`, or `Name`) for polymorphic members.
    `Optional<T>` and `vector<T>` unwrap to their element type.
*/
template <typename T>
std::string rncMember()
{
    using Type = std::decay_t<T>;

    if constexpr (std::is_same_v<Type, bool>)
    {
        return "Bool";
    }
    else if constexpr (std::is_same_v<Type, std::string> ||
                       std::is_same_v<Type, dom::String>)
    {
        return "text";
    }
    else if constexpr (std::is_integral_v<Type>)
    {
        return "text";
    }
    else if constexpr (std::is_same_v<Type, SymbolID>)
    {
        return "SymbolID";
    }
    else if constexpr (std::is_base_of_v<ExprInfo, Type>)
    {
        return "text";
    }
    else if constexpr (describe::has_describe_enumerators<Type>::value)
    {
        return patternName<Type>();
    }
    else if constexpr (mrdocs::detail::is_optional_v<Type>)
    {
        return rncMember<typename Type::value_type>();
    }
    else if constexpr (mrdocs::detail::is_vector_v<Type>)
    {
        return rncMember<typename Type::value_type>();
    }
    else if constexpr (mrdocs::detail::is_polymorphic_v<Type>)
    {
        using V = typename Type::value_type;
        if constexpr (std::is_same_v<V, mrdocs::Type>)
        {
            return "AnyType";
        }
        else if constexpr (std::is_same_v<V, mrdocs::Name>)
        {
            // XMLWriter's writePolymorphic for Polymorphic<Name>
            // falls into the generic case where T is deduced to the
            // base `Name` (Polymorphic::operator* returns a base
            // reference), so the element opened is always <name>
            // with the base's described members.
            return "Name";
        }
        else if constexpr (std::is_same_v<V, mrdocs::TParam>)
        {
            return "AnyTParam";
        }
        else if constexpr (std::is_same_v<V, mrdocs::TArg>)
        {
            return "AnyTArg";
        }
        else if constexpr (std::is_same_v<V, doc::Block>)
        {
            return "BlockNode";
        }
        else if constexpr (std::is_same_v<V, doc::Inline>)
        {
            return "InlineNode";
        }
        else
        {
            return "text";
        }
    }
    else if constexpr (describe::has_describe_members<Type>::value)
    {
        return patternName<Type>();
    }
    else
    {
        return "text";
    }
}

/** RNC quantifier for a member type.

    @tparam T The C++ member type.
    @return `"*"` for `std::vector<T>` (zero or more occurrences),
    `"?"` for everything else (optional, since XMLWriter skips
    empty values).
*/
template <typename T>
std::string rncQuantifier()
{
    using Type = std::decay_t<T>;

    if constexpr (mrdocs::detail::is_vector_v<Type>)
    {
        return "*";
    }
    else
    {
        // Almost everything in the XML output is optional
        // (XMLWriter's writeElement skips empty values).
        return "?";
    }
}

/** True if this member type is skipped by XMLWriter.

    Non-described enums and non-described structs without
    explicit handling in XMLWriter::write() (NoexceptInfo,
    ExplicitInfo) fall through without producing output.
*/
template <typename M>
inline constexpr bool rnc_is_omitted_v =
    (std::is_enum_v<M> && !describe::has_describe_enumerators<M>::value) ||
    std::is_same_v<M, NoexceptInfo> ||
    std::is_same_v<M, ExplicitInfo>;

/** True if a described struct T (XMLWriter uses tagName<T> as the
    element name, dropping the member name). */
template <typename M>
inline constexpr bool rnc_is_compound_v =
    describe::has_describe_members<std::decay_t<M>>::value &&
    !describe::has_describe_enumerators<std::decay_t<M>>::value &&
    !mrdocs::detail::is_optional_v<std::decay_t<M>> &&
    !mrdocs::detail::is_vector_v<std::decay_t<M>> &&
    !mrdocs::detail::is_polymorphic_v<std::decay_t<M>>;

/** Unwrap Optional<T> and vector<T> to T; otherwise leaves T unchanged.
    Used to check whether the "inner" type is a compound described struct.
*/
template <typename M>
struct unwrap_type {
    /// The unwrapped inner type (equal to `std::decay_t<M>` by default).
    using type = std::decay_t<M>;
};

/** Partial specialization that unwraps `Optional<T>` to `T`. */
template <typename T>
struct unwrap_type<Optional<T>> {
    /// The unwrapped inner type.
    using type = std::decay_t<T>;
};

/** Partial specialization that unwraps `std::vector<T, A>` to `T`. */
template <typename T, typename A>
struct unwrap_type<std::vector<T, A>> {
    /// The unwrapped element type.
    using type = std::decay_t<T>;
};

/** Alias for `unwrap_type<M>::type`.

    @tparam M The C++ member type to unwrap.
*/
template <typename M>
using unwrap_type_t = typename unwrap_type<std::decay_t<M>>::type;

/** True if the member wraps (via Optional/vector) a described struct. */
template <typename M>
inline constexpr bool rnc_is_wrapped_compound_v =
    (mrdocs::detail::is_optional_v<std::decay_t<M>> ||
     mrdocs::detail::is_vector_v<std::decay_t<M>>) &&
    rnc_is_compound_v<unwrap_type_t<M>>;

/** True if the member is a Polymorphic<T> (uses AnyXxx pattern). */
template <typename M>
inline constexpr bool rnc_is_polymorphic_v =
    mrdocs::detail::is_polymorphic_v<std::decay_t<M>>;

/** True if the member is a vector of Polymorphic<T>. */
template <typename M>
struct is_vector_of_polymorphic : std::false_type {};

/** Specialization recognizing `std::vector<Polymorphic<T>, A>`. */
template <typename T, typename A>
struct is_vector_of_polymorphic<std::vector<Polymorphic<T>, A>>
    : std::true_type {};

/** Variable template alias for @ref is_vector_of_polymorphic. */
template <typename M>
inline constexpr bool rnc_is_vector_of_polymorphic_v =
    is_vector_of_polymorphic<std::decay_t<M>>::value;

/** True if the member is an Optional<Polymorphic<T>>.

    XMLWriter's writeElement strips the outer Optional and dispatches
    on the polymorphic value, so the member-name wrapper is not used.
*/
template <typename M>
struct is_optional_of_polymorphic : std::false_type {};

/** Specialization recognizing `Optional<Polymorphic<T>>`. */
template <typename T>
struct is_optional_of_polymorphic<Optional<Polymorphic<T>>>
    : std::true_type {};

/** Variable template alias for @ref is_optional_of_polymorphic. */
template <typename M>
inline constexpr bool rnc_is_optional_of_polymorphic_v =
    is_optional_of_polymorphic<std::decay_t<M>>::value;

} // namespace rnc_detail

// ---------------------------------------------------------------
// RNC emitter
// ---------------------------------------------------------------

/** Streaming builder for the MrDocs RELAX NG Compact (RNC) schema.

    Accumulates pattern definitions and finally returns the full
    grammar via @ref build. Pattern definitions are produced by
    reflecting over described types (enums, structs, and the
    polymorphic node families).
*/
class RncEmitter
{
    std::string out_;
    int indent_ = 0;

    void line(std::string_view s = {})
    {
        if (!s.empty())
        {
            out_ += std::string(indent_ * 4, ' ');
            out_ += s;
        }
        out_ += '\n';
    }

public:
    /** Emit RNC for a described enum. */
    template <typename E>
    void emitEnum()
    {
        std::string def = rnc_detail::patternName<E>();
        def += " = ";
        bool first = true;
        describe::for_each(
            describe::describe_enumerators<E>{},
            [&](auto const& D) {
                if (!first)
                {
                    def += " | ";
                }
                first = false;
                def += '"';
                def += toKebabCase(D.name);
                def += '"';
            });
        line(def);
    }

    /** Emit RNC for the members of a described type (bases + own). */
    template <typename T>
    void emitMembers()
    {
        // Base class members first.
        if constexpr (describe::has_describe_bases<T>::value)
        {
            describe::for_each(
                describe::describe_bases<T>{},
                [&](auto const& descriptor)
                {
                    using BaseType = typename std::decay_t<
                        decltype(descriptor)>::type;
                    emitMembers<BaseType>();
                });
        }

        // Own members.
        if constexpr (describe::has_describe_members<T>::value)
        {
            describe::for_each(
                describe::describe_members<T>{},
                [&](auto const& D)
                {
                    using M = std::decay_t<decltype(
                        std::declval<T>().*D.pointer)>;

                    // Skip types that XMLWriter silently omits.
                    if constexpr (rnc_detail::rnc_is_omitted_v<M>)
                    {
                        return;
                    }
                    // Polymorphic members: use AnyXxx pattern directly.
                    else if constexpr (rnc_detail::rnc_is_polymorphic_v<M>)
                    {
                        std::string l = rnc_detail::rncMember<M>();
                        l += rnc_detail::rncQuantifier<M>();
                        line(l + ",");
                    }
                    // Vectors of polymorphic: AnyXxx*.
                    else if constexpr (rnc_detail::rnc_is_vector_of_polymorphic_v<M>)
                    {
                        using Inner = typename std::decay_t<M>::value_type;
                        std::string l = rnc_detail::rncMember<Inner>();
                        l += "*,";
                        line(l);
                    }
                    // Optional<Polymorphic<X>>: AnyXxx? — XMLWriter
                    // unwraps both layers and dispatches to the
                    // polymorphic kind, so no member-name wrapper.
                    else if constexpr (rnc_detail::rnc_is_optional_of_polymorphic_v<M>)
                    {
                        using Inner = typename std::decay_t<M>::value_type;
                        std::string l = rnc_detail::rncMember<Inner>();
                        l += "?,";
                        line(l);
                    }
                    // Described structs (bare or wrapped in Optional/vector):
                    // XMLWriter uses the type's tag name, so we reference
                    // the pattern directly without an element wrapper.
                    else if constexpr (rnc_detail::rnc_is_compound_v<M> ||
                                       rnc_detail::rnc_is_wrapped_compound_v<M>)
                    {
                        using Inner = rnc_detail::unwrap_type_t<M>;
                        std::string l = rnc_detail::patternName<Inner>();
                        l += rnc_detail::rncQuantifier<M>();
                        l += ',';
                        line(l);
                    }
                    // Primitive/enum members: element <name> { type }?.
                    else
                    {
                        std::string l = "element ";
                        l += toKebabCase(D.name);
                        l += " { ";
                        l += rnc_detail::rncMember<M>();
                        l += " }";
                        l += rnc_detail::rncQuantifier<M>();
                        l += ',';
                        line(l);
                    }
                });
        }
    }

    /** Emit a group pattern for a described type (no element wrapper).

        Used for abstract base types like `Name` whose members are
        embedded directly in a containing element.
    */
    template <typename T>
    void emitGroupDef()
    {
        std::string pattern = rnc_detail::patternName<T>();
        line(pattern + " =");
        ++indent_;
        line("(");
        ++indent_;
        emitMembers<T>();
        auto pos = out_.rfind(',');
        if (pos != std::string::npos && pos > out_.size() - 20)
        {
            out_.erase(pos, 1);
        }
        --indent_;
        line(")");
        --indent_;
        line();
    }

    /** Emit a full element definition for a described type. */
    template <typename T>
    void emitElementDef()
    {
        std::string tag = rnc_detail::tagName<T>();
        std::string pattern = rnc_detail::patternName<T>();
        line(pattern + " =");
        ++indent_;
        line("element " + tag);
        line("{");
        ++indent_;
        emitMembers<T>();
        // Remove trailing comma from last member.
        auto pos = out_.rfind(',');
        if (pos != std::string::npos && pos > out_.size() - 20)
        {
            out_.erase(pos, 1);
        }
        --indent_;
        line("}");
        --indent_;
        line();
    }

    /** Build the complete RNC schema.

        @return The full grammar text, ready to be written to
        `mrdocs.rnc`.
    */
    std::string build()
    {
        // -------------------------------------------------------
        // Header
        // -------------------------------------------------------
        line("#");
        line("# Auto-generated by mrdocs --schemas.");
        line("# Do not edit manually.");
        line("#");
        line("# https://relaxng.org/compact-tutorial-20030326.html");
        line("#");
        line();
        line("namespace xsi= \"http://www.w3.org/2001/XMLSchema-instance\"");
        line();
        line("grammar");
        line("{");

        indent_ = 1;

        // -------------------------------------------------------
        // Root
        // -------------------------------------------------------
        line("start = Mrdocs | Tagfile");
        line();
        line("Mrdocs =");
        line("    element mrdocs");
        line("    {");
        line("        attribute xsi:noNamespaceSchemaLocation { text }?,");
        line("        AnySymbol*");
        line("    }");
        line();
        line("Tagfile =");
        line("    element tagfile");
        line("    {");
        line("        TagCompound+");
        line("    }");
        line();
        line("TagCompound =");
        line("    element compound");
        line("    {");
        line("        attribute kind { \"namespace\" | \"class\" },");
        line("        element name { text },");
        line("        element filename { text },");
        line("        (TagClass | TagMember)*");
        line("    }");
        line();
        line("TagClass =");
        line("    element class");
        line("    {");
        line("        attribute kind { \"class\" },");
        line("        text");
        line("    }");
        line();
        line("TagMember =");
        line("    element member");
        line("    {");
        line("        attribute kind { \"function\" },");
        line("        element type { text },");
        line("        element name { text },");
        line("        element anchorfile { text },");
        line("        element anchor { text },");
        line("        element arglist { text }");
        line("    }");
        line();

        // -------------------------------------------------------
        // Common types
        // -------------------------------------------------------
        line("#---------------------------------------------");
        line("# Common types");
        line("#---------------------------------------------");
        line();
        line("SymbolID = text   # Base64-encoded");
        line();
        line("Bool = \"1\"");
        line();

        // -------------------------------------------------------
        // Described enums
        // -------------------------------------------------------
        line("#---------------------------------------------");
        line("# Enums");
        line("#---------------------------------------------");
        line();
        emitEnum<ExtractionMode>();
        emitEnum<FunctionClass>();
        emitEnum<RecordKeyKind>();
        emitEnum<UsingClass>();
        emitEnum<AccessKind>();
        emitEnum<ConstexprKind>();
        emitEnum<StorageClassKind>();
        emitEnum<SymbolKind>();
        emitEnum<NameKind>();
        emitEnum<TArgKind>();
        emitEnum<TParamKind>();
        emitEnum<TParamKeyKind>();
        emitEnum<doc::AdmonitionKind>();
        emitEnum<doc::ParamDirection>();
        emitEnum<ListKind>();
        emitEnum<TableAlignmentKind>();
        emitEnum<doc::BlockKind>();
        emitEnum<doc::InlineKind>();
        line();

        // -------------------------------------------------------
        // Types (polymorphic)
        // -------------------------------------------------------
        line("#---------------------------------------------");
        line("# Types (polymorphic)");
        line("#---------------------------------------------");
        line();
        #define INFO(X) emitElementDef<X##Type>();
        #include <mrdocs/Metadata/Type/TypeNodes.inc>

        line("AnyType =");
        {
            std::string choice = "    ";
            bool first = true;
            #define INFO(X) \
                if (!first) { choice += " |\n    "; } \
                first = false; \
                choice += rnc_detail::patternName<X##Type>();
            #include <mrdocs/Metadata/Type/TypeNodes.inc>
            line(choice);
        }
        line();

        // -------------------------------------------------------
        // Names
        // -------------------------------------------------------
        line("#---------------------------------------------");
        line("# Names");
        line("#---------------------------------------------");
        line();
        #define INFO(X) emitElementDef<X##Name>();
        #include <mrdocs/Metadata/Name/NameNodes.inc>

        line("AnyName =");
        {
            std::string choice = "    ";
            bool first = true;
            #define INFO(X) \
                if (!first) { choice += " |\n    "; } \
                first = false; \
                choice += rnc_detail::patternName<X##Name>();
            #include <mrdocs/Metadata/Name/NameNodes.inc>
            line(choice);
        }
        line();

        // -------------------------------------------------------
        // Template parameters
        // -------------------------------------------------------
        line("#---------------------------------------------");
        line("# Template parameters (polymorphic)");
        line("#---------------------------------------------");
        line();
        #define INFO(X) emitElementDef<X##TParam>();
        #include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>

        line("AnyTParam = ");
        {
            std::string choice = "    ";
            bool first = true;
            #define INFO(X) \
                if (!first) { choice += " | "; } \
                first = false; \
                choice += rnc_detail::patternName<X##TParam>();
            #include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>
            line(choice);
        }
        line();

        // -------------------------------------------------------
        // Template arguments
        // -------------------------------------------------------
        line("#---------------------------------------------");
        line("# Template arguments (polymorphic)");
        line("#---------------------------------------------");
        line();
        #define INFO(X) emitElementDef<X##TArg>();
        #include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>

        line("AnyTArg = ");
        {
            std::string choice = "    ";
            bool first = true;
            #define INFO(X) \
                if (!first) { choice += " | "; } \
                first = false; \
                choice += rnc_detail::patternName<X##TArg>();
            #include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>
            line(choice);
        }
        line();

        // -------------------------------------------------------
        // Supporting types
        // -------------------------------------------------------
        line("#---------------------------------------------");
        line("# Supporting types");
        line("#---------------------------------------------");
        line();
        emitElementDef<TemplateInfo>();
        emitElementDef<Param>();
        emitElementDef<Location>();
        emitElementDef<SourceInfo>();
        emitElementDef<BaseInfo>();
        emitElementDef<FriendInfo>();
        emitElementDef<NamespaceTranche>();
        emitElementDef<RecordInterface>();
        emitElementDef<RecordTranche>();
        emitElementDef<mrdocs::Name>();
        emitElementDef<doc::InlineContainer>();
        emitElementDef<doc::ListItem>();
        emitElementDef<doc::DefinitionListItem>();
        emitElementDef<doc::TableRow>();
        emitElementDef<doc::TableCell>();

        // -------------------------------------------------------
        // Symbols
        // -------------------------------------------------------
        line("#---------------------------------------------");
        line("# Symbols");
        line("#---------------------------------------------");
        line();
        #define INFO(X) emitElementDef<X##Symbol>();
        #include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

        line("AnySymbol =");
        {
            std::string choice = "    ";
            bool first = true;
            #define INFO(X) \
                if (!first) { choice += " |\n    "; } \
                first = false; \
                choice += rnc_detail::patternName<X##Symbol>();
            #include <mrdocs/Metadata/Symbol/SymbolNodes.inc>
            line(choice);
        }
        line();

        // -------------------------------------------------------
        // DocComment
        // -------------------------------------------------------
        line("#---------------------------------------------");
        line("# DocComment");
        line("#---------------------------------------------");
        line();
        emitElementDef<DocComment>();

        // Block nodes
        line("#---------------------------------------------");
        line("# Block nodes");
        line("#---------------------------------------------");
        line();
        #define INFO(X) emitElementDef<doc::X##Block>();
        #include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

        line("BlockNode =");
        {
            std::string choice = "    ";
            bool first = true;
            #define INFO(X) \
                if (!first) { choice += " |\n    "; } \
                first = false; \
                choice += rnc_detail::patternName<doc::X##Block>();
            #include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
            line(choice);
        }
        line();

        // Inline nodes
        line("#---------------------------------------------");
        line("# Inline nodes");
        line("#---------------------------------------------");
        line();
        #define INFO(X) emitElementDef<doc::X##Inline>();
        #include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

        line("InlineNode =");
        {
            std::string choice = "    ";
            bool first = true;
            #define INFO(X) \
                if (!first) { choice += " |\n    "; } \
                first = false; \
                choice += rnc_detail::patternName<doc::X##Inline>();
            #include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>
            line(choice);
        }
        line();

        // -------------------------------------------------------
        // Close grammar
        // -------------------------------------------------------
        indent_ = 0;
        line("}");

        return out_;
    }
};

/** Build the complete RNC schema string.

    @return The full RELAX NG Compact grammar for the MrDocs XML
    output, suitable for writing directly to `mrdocs.rnc`.
*/
inline std::string
buildRncSchema()
{
    RncEmitter emitter;
    return emitter.build();
}

} // mrdocs::schema

#endif // MRDOCS_LIB_SCHEMAS_RNCSCHEMAWRITER_HPP
