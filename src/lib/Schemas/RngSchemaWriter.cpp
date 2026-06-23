//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Schemas/RngSchemaWriter.hpp>
#include <lib/Support/Xml.hpp>
#include <mrdocs/Metadata.hpp>
#include <mrdocs/Support/Describe.hpp>
#include <mrdocs/Support/EnumToString.hpp>
#include <mrdocs/Support/MapReflectedType.hpp>
#include <mrdocs/Support/String.hpp>
#include <mrdocs/Support/TypeTraits.hpp>
#include <llvm/Support/raw_ostream.h>
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace mrdocs::schema {

namespace {

// ---------------------------------------------------------------
// Helpers: type -> tag/pattern names
// ---------------------------------------------------------------

/* Remove `suffix` from the end of `s`, if present.

    Strict greater-than guards against emptying the result when
    a base struct (e.g. the bare `Type`) shares its name with a
    listed suffix.
*/
std::string
removeSuffix(std::string_view s, std::string_view suffix)
{
    return s.size() > suffix.size() &&
           s.substr(s.size() - suffix.size()) == suffix
        ? std::string(s.substr(0, s.size() - suffix.size()))
        : std::string(s);
}

/* XML element tag name for a described type, mirroring XMLWriter.

    The kebab-case tag name produced by stripping well-known
    suffixes (Symbol, Info, TypeInfo, Type, TParam, TArg, Block,
    Inline) from the type's unqualified name. TParam-derived
    types are suffixed with "-tparam", TArg-derived types with
    "-targ", and Name-derived types with "-name", to mirror
    XMLWriter's polymorphic dispatch.

    The "Name" suffix on the type name itself is intentionally not
    stripped: the bare base struct `Name` stays "name" and concrete
    name types like `IdentifierName` stay "identifier-name".
*/
template <typename T>
std::string tagName()
{
    std::string raw(readableTypeName<T>());
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

/* RNG pattern name (PascalCase, used for <define name="..."> and
   <ref name="...">). */
template <typename T>
std::string patternName()
{
    return std::string(readableTypeName<T>());
}

// ---------------------------------------------------------------
// Member-type traits
// ---------------------------------------------------------------

/* True if this member type is silently dropped by XMLWriter.

   Non-described enums (TypeKind, TParamKind, TArgKind, ...)
   fall through XMLWriter::writeElement without producing output.
   The polymorphic-dispatch tag name itself encodes the kind, so
   the enum value isn't re-emitted as an element.
*/
template <typename M>
inline constexpr bool rng_is_omitted_v =
    std::is_enum_v<M> && !describe::has_describe_enumerators<M>::value;

/* True if `M` is a described struct that XMLWriter wraps with
   `tagName<M>()` (dropping the member name). */
template <typename M>
inline constexpr bool rng_is_compound_v =
    describe::has_describe_members<std::decay_t<M>>::value &&
    !describe::has_describe_enumerators<std::decay_t<M>>::value &&
    !mrdocs::detail::is_optional_v<std::decay_t<M>> &&
    !mrdocs::detail::is_vector_v<std::decay_t<M>> &&
    !mrdocs::detail::is_polymorphic_v<std::decay_t<M>>;

/* Unwrap Optional<T> and vector<T> to T; otherwise unchanged. */
template <typename M>
struct unwrap_type
{
    using type = std::decay_t<M>;
};
template <typename T>
struct unwrap_type<Optional<T>>
{
    using type = std::decay_t<T>;
};
template <typename T, typename A>
struct unwrap_type<std::vector<T, A>>
{
    using type = std::decay_t<T>;
};
template <typename M>
using unwrap_type_t = typename unwrap_type<std::decay_t<M>>::type;

/* True if the member wraps (via Optional/vector) a described struct. */
template <typename M>
inline constexpr bool rng_is_wrapped_compound_v =
    (mrdocs::detail::is_optional_v<std::decay_t<M>> ||
     mrdocs::detail::is_vector_v<std::decay_t<M>>) &&
    rng_is_compound_v<unwrap_type_t<M>>;

/* True if the member is a Polymorphic<T>. */
template <typename M>
inline constexpr bool rng_is_polymorphic_v =
    mrdocs::detail::is_polymorphic_v<std::decay_t<M>>;

/* True if the member is a vector<Polymorphic<T>>. */
template <typename M>
struct is_vector_of_polymorphic : std::false_type {};
template <typename T, typename A>
struct is_vector_of_polymorphic<std::vector<Polymorphic<T>, A>>
    : std::true_type {};
template <typename M>
inline constexpr bool rng_is_vector_of_polymorphic_v =
    is_vector_of_polymorphic<std::decay_t<M>>::value;

/* True if the member is an Optional<Polymorphic<T>>.

   XMLWriter's writeElement strips the outer Optional and dispatches
   on the polymorphic value, so the member-name wrapper is not used.
*/
template <typename M>
struct is_optional_of_polymorphic : std::false_type {};
template <typename T>
struct is_optional_of_polymorphic<Optional<Polymorphic<T>>>
    : std::true_type {};
template <typename M>
inline constexpr bool rng_is_optional_of_polymorphic_v =
    is_optional_of_polymorphic<std::decay_t<M>>::value;

// ---------------------------------------------------------------
// Reference-name (`<ref name="...">`) for a member's content type
// ---------------------------------------------------------------

/* Pattern reference (or primitive) for the content of a member.

   Returns the name to be used in `<ref name="...">` (or one of the
   built-in primitives "Bool" / "SymbolID" / "text"). Polymorphic
   members map to the "AnyXxx" choice pattern. Optional/vector
   members unwrap to their element type.
*/
template <typename T>
std::string refName()
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
        return refName<typename Type::value_type>();
    }
    else if constexpr (mrdocs::detail::is_vector_v<Type>)
    {
        return refName<typename Type::value_type>();
    }
    else if constexpr (mrdocs::detail::is_polymorphic_v<Type>)
    {
        using V = typename Type::value_type;
        if constexpr (std::is_same_v<V, mrdocs::Type>) return "AnyType";
        else if constexpr (std::is_same_v<V, mrdocs::Name>) return "AnyName";
        else if constexpr (std::is_same_v<V, mrdocs::TParam>) return "AnyTParam";
        else if constexpr (std::is_same_v<V, mrdocs::TArg>) return "AnyTArg";
        else if constexpr (std::is_same_v<V, doc::Block>) return "BlockNode";
        else if constexpr (std::is_same_v<V, doc::Inline>) return "InlineNode";
        else if constexpr (std::is_same_v<V, mrdocs::Attribute>) return "AnyAttribute";
        else return "text";
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

// ---------------------------------------------------------------
// RngEmitter
// ---------------------------------------------------------------

/* Streaming builder for the MrDocs RELAX NG (XML syntax) schema.

   Emits a complete `<grammar>` document directly via
   @ref mrdocs::xml::XmlEmitter; pattern definitions are produced
   by reflecting over described types.
*/
class RngEmitter
{
    xml::XmlEmitter xml_;
    llvm::raw_ostream& os_;

public:
    explicit
    RngEmitter(llvm::raw_ostream& os) noexcept
        : xml_(os)
        , os_(os)
    {
    }

    void build();

private:
    // ------------------------------------------------------------
    // Primitive emitters
    // ------------------------------------------------------------

    /* Emit `<text/>`. */
    void emitText()
    {
        xml_.write("text");
    }

    /* Emit `<ref name="X"/>`. */
    void emitRef(std::string_view name)
    {
        std::array<xml::XmlAttribute, 1> const attrs{{
            { "name", std::string(name), true }
        }};
        xml_.write("ref", {}, attrs);
    }

    /* Emit `<value>X</value>`. */
    void emitValue(std::string_view v)
    {
        xml_.write("value", v);
    }

    // ------------------------------------------------------------
    // Open/close wrappers (used with body lambdas)
    // ------------------------------------------------------------

    /* Open `<TAG name="NAME">` and increase indent. */
    void openNamed(std::string_view tag, std::string_view name)
    {
        std::array<xml::XmlAttribute, 1> const attrs{{
            { "name", std::string(name), true }
        }};
        xml_.open(tag, attrs);
    }

    template <typename F>
    void wrap(std::string_view tag, F&& body)
    {
        xml_.open(tag);
        body();
        xml_.close(tag);
    }

    template <typename F>
    void wrapNamed(std::string_view tag, std::string_view name, F&& body)
    {
        openNamed(tag, name);
        body();
        xml_.close(tag);
    }

    // ------------------------------------------------------------
    // Member emission
    // ------------------------------------------------------------

    /* Emit the schema fragment for a member named `domName` of
       a described struct, whose member type is `M`.

       Polymorphic and described-struct members reference the
       appropriate pattern directly (no member-name wrapper,
       since XMLWriter drops it); plain scalars and enums get an
       `<element name="...">` wrapper named after the kebab-cased
       member name.
    */
    template <typename M>
    void emitMember(std::string_view domName)
    {
        using Inner = unwrap_type_t<M>;

        // Polymorphic: use the AnyXxx choice (or "Name" / etc.) directly.
        if constexpr (rng_is_polymorphic_v<M>)
        {
            wrap("optional", [&] { emitRef(refName<M>()); });
        }
        // vector<Polymorphic<X>>: <zeroOrMore><ref name="AnyXxx"/></zeroOrMore>.
        else if constexpr (rng_is_vector_of_polymorphic_v<M>)
        {
            using Poly = typename std::decay_t<M>::value_type;
            wrap("zeroOrMore", [&] { emitRef(refName<Poly>()); });
        }
        // Optional<Polymorphic<X>>: <optional><ref name="AnyXxx"/></optional>.
        else if constexpr (rng_is_optional_of_polymorphic_v<M>)
        {
            using Poly = typename std::decay_t<M>::value_type;
            wrap("optional", [&] { emitRef(refName<Poly>()); });
        }
        // Described struct (bare or wrapped): reference the pattern
        // directly (XMLWriter uses tagName<T> as the wrapper, not the
        // member name).
        else if constexpr (rng_is_compound_v<M> ||
                           rng_is_wrapped_compound_v<M>)
        {
            if constexpr (mrdocs::detail::is_vector_v<std::decay_t<M>>)
            {
                wrap("zeroOrMore",
                    [&] { emitRef(patternName<Inner>()); });
            }
            else
            {
                wrap("optional",
                    [&] { emitRef(patternName<Inner>()); });
            }
        }
        // Plain scalar/enum: <element name="..."> ... </element> wrapped
        // in <optional>/<zeroOrMore>.
        else
        {
            std::string const tag = std::string(domName);
            std::string const ref = refName<M>();
            auto const elementBody = [&] {
                wrapNamed("element", tag, [&] {
                    if (ref == "text")
                    {
                        emitText();
                    }
                    else
                    {
                        emitRef(ref);
                    }
                });
            };
            if constexpr (mrdocs::detail::is_vector_v<std::decay_t<M>>)
            {
                wrap("zeroOrMore", elementBody);
            }
            else
            {
                wrap("optional", elementBody);
            }
        }
    }

    /* Emit each described member of T (bases first, then own). */
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
                    if constexpr (describe::has_describe_members<BaseType>::value)
                    {
                        emitMembers<BaseType>();
                    }
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
                    if constexpr (rng_is_omitted_v<M>)
                    {
                        return;
                    }
                    else
                    {
                        std::string const dom = toKebabCase(D.name);
                        emitMember<M>(dom);
                    }
                });
        }
    }

    // ------------------------------------------------------------
    // Pattern definitions
    // ------------------------------------------------------------

    /* Emit `<define name="EnumName"><choice><value>x</value>...</choice></define>`. */
    template <typename E>
    void defineEnum()
    {
        wrapNamed("define", patternName<E>(), [&] {
            wrap("choice", [&] {
                describe::for_each(
                    describe::describe_enumerators<E>{},
                    [&](auto const& D)
                    {
                        emitValue(toKebabCase(D.name));
                    });
            });
        });
    }

    /* Emit `<define name="X"><element name="x">...members...</element></define>`. */
    template <typename T>
    void defineElement()
    {
        wrapNamed("define", patternName<T>(), [&] {
            wrapNamed("element", tagName<T>(), [&] {
                emitMembers<T>();
            });
        });
    }

    // ------------------------------------------------------------
    // Polymorphic-family group helpers
    // ------------------------------------------------------------

    /* Emit `<define name="ChoiceName"><choice>refs...</choice></define>`. */
    template <typename Refs>
    void defineChoice(std::string_view choiceName, Refs&& refs)
    {
        wrapNamed("define", choiceName, [&] {
            wrap("choice", refs);
        });
    }

    // ------------------------------------------------------------
    // Top-level sections
    // ------------------------------------------------------------

    void emitPrologue();
    void emitRootElements();
    void emitCommonTypes();
    void emitEnums();
    void emitTypes();
    void emitNames();
    void emitTParams();
    void emitTArgs();
    void emitAttributes();
    void emitSupportingTypes();
    void emitSymbols();
    void emitDocComment();
    void emitBlocks();
    void emitInlines();
};

// ---------------------------------------------------------------
// Top-level sections
// ---------------------------------------------------------------

void
RngEmitter::
emitPrologue()
{
    os_ << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<!-- Auto-generated by mrdocs. Do not edit. -->\n";
    std::array<xml::XmlAttribute, 2> const grammarAttrs{{
        { "xmlns", "http://relaxng.org/ns/structure/1.0", true },
        { "datatypeLibrary", "http://www.w3.org/2001/XMLSchema-datatypes", true }
    }};
    xml_.open("grammar", grammarAttrs);
}

void
RngEmitter::
emitRootElements()
{
    // start = Mrdocs | Tagfile
    wrap("start", [&] {
        wrap("choice", [&] {
            emitRef("Mrdocs");
            emitRef("Tagfile");
        });
    });

    // Mrdocs = element mrdocs { xsi:schema-loc?, AnySymbol* }
    wrapNamed("define", "Mrdocs", [&] {
        wrapNamed("element", "mrdocs", [&] {
            wrap("optional", [&] {
                std::array<xml::XmlAttribute, 2> const attrs{{
                    { "name", "noNamespaceSchemaLocation", true },
                    { "ns", "http://www.w3.org/2001/XMLSchema-instance", true }
                }};
                xml_.open("attribute", attrs);
                emitText();
                xml_.close("attribute");
            });
            wrap("zeroOrMore", [&] { emitRef("AnySymbol"); });
        });
    });

    // Tagfile family
    wrapNamed("define", "Tagfile", [&] {
        wrapNamed("element", "tagfile", [&] {
            wrap("oneOrMore", [&] { emitRef("TagCompound"); });
        });
    });

    wrapNamed("define", "TagCompound", [&] {
        wrapNamed("element", "compound", [&] {
            wrapNamed("attribute", "kind", [&] {
                wrap("choice", [&] {
                    emitValue("namespace");
                    emitValue("class");
                });
            });
            wrapNamed("element", "name", [&] { emitText(); });
            wrapNamed("element", "filename", [&] { emitText(); });
            wrap("zeroOrMore", [&] {
                wrap("choice", [&] {
                    emitRef("TagClass");
                    emitRef("TagMember");
                });
            });
        });
    });

    wrapNamed("define", "TagClass", [&] {
        wrapNamed("element", "class", [&] {
            wrapNamed("attribute", "kind", [&] { emitValue("class"); });
            emitText();
        });
    });

    wrapNamed("define", "TagMember", [&] {
        wrapNamed("element", "member", [&] {
            wrapNamed("attribute", "kind", [&] { emitValue("function"); });
            wrapNamed("element", "type",       [&] { emitText(); });
            wrapNamed("element", "name",       [&] { emitText(); });
            wrapNamed("element", "anchorfile", [&] { emitText(); });
            wrapNamed("element", "anchor",     [&] { emitText(); });
            wrapNamed("element", "arglist",    [&] { emitText(); });
        });
    });
}

void
RngEmitter::
emitCommonTypes()
{
    wrapNamed("define", "SymbolID", [&] { emitText(); });
    wrapNamed("define", "Bool", [&] { emitValue("1"); });
}

void
RngEmitter::
emitEnums()
{
    defineEnum<ExtractionMode>();
    defineEnum<FunctionClass>();
    defineEnum<RecordKeyKind>();
    defineEnum<UsingClass>();
    defineEnum<AccessKind>();
    defineEnum<ConstexprKind>();
    defineEnum<StorageClassKind>();
    defineEnum<SymbolKind>();
    defineEnum<NameKind>();
    defineEnum<TArgKind>();
    defineEnum<TParamKind>();
    defineEnum<TParamKeyKind>();
    defineEnum<doc::AdmonitionKind>();
    defineEnum<doc::ParamDirection>();
    defineEnum<ListKind>();
    defineEnum<TableAlignmentKind>();
    defineEnum<doc::BlockKind>();
    defineEnum<doc::InlineKind>();
}

void
RngEmitter::
emitTypes()
{
    #define INFO(X) defineElement<X##Type>();
    #include <mrdocs/Metadata/Type/TypeNodes.inc>

    defineChoice("AnyType", [&] {
        #define INFO(X) emitRef(patternName<X##Type>());
        #include <mrdocs/Metadata/Type/TypeNodes.inc>
    });
}

void
RngEmitter::
emitNames()
{
    #define INFO(X) defineElement<X##Name>();
    #include <mrdocs/Metadata/Name/NameNodes.inc>

    defineChoice("AnyName", [&] {
        #define INFO(X) emitRef(patternName<X##Name>());
        #include <mrdocs/Metadata/Name/NameNodes.inc>
    });
}

void
RngEmitter::
emitTParams()
{
    #define INFO(X) defineElement<X##TParam>();
    #include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>

    defineChoice("AnyTParam", [&] {
        #define INFO(X) emitRef(patternName<X##TParam>());
        #include <mrdocs/Metadata/TParam/TParamInfoNodes.inc>
    });
}

void
RngEmitter::
emitTArgs()
{
    #define INFO(X) defineElement<X##TArg>();
    #include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>

    defineChoice("AnyTArg", [&] {
        #define INFO(X) emitRef(patternName<X##TArg>());
        #include <mrdocs/Metadata/TArg/TArgInfoNodes.inc>
    });
}

void
RngEmitter::
emitAttributes()
{
    #define INFO(X) defineElement<X##Attribute>();
    #include <mrdocs/Metadata/Attribute/AttributeNodes.inc>

    defineChoice("AnyAttribute", [&] {
        #define INFO(X) emitRef(patternName<X##Attribute>());
        #include <mrdocs/Metadata/Attribute/AttributeNodes.inc>
    });
}

void
RngEmitter::
emitSupportingTypes()
{
    defineElement<TemplateInfo>();
    defineElement<Param>();
    defineElement<Location>();
    defineElement<SourceInfo>();
    defineElement<BaseInfo>();
    defineElement<FriendInfo>();
    defineElement<NamespaceTranche>();
    defineElement<RecordInterface>();
    defineElement<RecordTranche>();
    // Bare Name (not Polymorphic<Name>): used for vector<Name>
    // members like NamespaceSymbol::UsingDirectives, where
    // XMLWriter emits each entry as <name>...base members...</name>.
    defineElement<mrdocs::Name>();
    defineElement<doc::InlineContainer>();
    defineElement<doc::ListItem>();
    defineElement<doc::DefinitionListItem>();
    defineElement<doc::TableRow>();
    defineElement<doc::TableCell>();
}

void
RngEmitter::
emitSymbols()
{
    #define INFO(X) defineElement<X##Symbol>();
    #include <mrdocs/Metadata/Symbol/SymbolNodes.inc>

    defineChoice("AnySymbol", [&] {
        #define INFO(X) emitRef(patternName<X##Symbol>());
        #include <mrdocs/Metadata/Symbol/SymbolNodes.inc>
    });
}

void
RngEmitter::
emitDocComment()
{
    defineElement<DocComment>();
}

void
RngEmitter::
emitBlocks()
{
    #define INFO(X) defineElement<doc::X##Block>();
    #include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>

    defineChoice("BlockNode", [&] {
        #define INFO(X) emitRef(patternName<doc::X##Block>());
        #include <mrdocs/Metadata/DocComment/Block/BlockNodes.inc>
    });
}

void
RngEmitter::
emitInlines()
{
    #define INFO(X) defineElement<doc::X##Inline>();
    #include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>

    defineChoice("InlineNode", [&] {
        #define INFO(X) emitRef(patternName<doc::X##Inline>());
        #include <mrdocs/Metadata/DocComment/Inline/InlineNodes.inc>
    });
}

void
RngEmitter::
build()
{
    emitPrologue();
    emitRootElements();
    emitCommonTypes();
    emitEnums();
    emitTypes();
    emitNames();
    emitTParams();
    emitTArgs();
    emitAttributes();
    emitSupportingTypes();
    emitSymbols();
    emitDocComment();
    emitBlocks();
    emitInlines();
    xml_.close("grammar");
}

} // (anon)

std::string
buildRngSchema()
{
    std::string buf;
    llvm::raw_string_ostream os(buf);
    RngEmitter emitter(os);
    emitter.build();
    return buf;
}

} // mrdocs::schema
