//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_SCHEMAS_DOMDESCRIPTIONS_HPP
#define MRDOCS_LIB_SCHEMAS_DOMDESCRIPTIONS_HPP

#include <mrdocs/Support/Assert.hpp>
#include <string_view>

/** Hand-curated descriptions for the JSON Schema produced by
    @ref DomSchemaWriter.

    The DOM that Handlebars templates see is a projection of the
    C++ corpus types. Its field names and shapes are derived
    automatically by reflection, but the *meaning* of each field
    cannot be inferred from the C++ types alone (e.g. the C++ doc
    comment describes the C++ class, not the DOM projection;
    the synthesized `class` field has no C++ counterpart at all).

    This header carries a small lookup table indexed by
    `(typeName, memberName)`. The schema writer consults it when
    emitting each `$defs` entry and each property; matched entries
    become `description` fields in the JSON Schema and propagate
    automatically to any human-readable rendering of it.

    A `memberName` of `""` denotes the type-level description.
*/
namespace mrdocs::schema {

/** A single description entry.

    `type` matches the readable type name produced by
    `readableTypeName<T>()` (e.g. `"FunctionSymbol"`,
    `"NamedType"`).

    `member` matches the DOM-normalized field name (the
    C++ identifier with its first letter lowercased), or
    `""` for a description of the type itself.
*/
struct DomDescription
{
    std::string_view type;
    std::string_view member;
    std::string_view text;
};

/** Retrieve the description for `(type, member)`.

    Every described DOM type the schema writer emits, and every
    described member of those types, is required to have an
    entry; a missing one is treated as a developer mistake and
    aborts via `MRDOCS_UNREACHABLE`.

    @return The description text.
*/
inline std::string_view
findDomDescription(
    std::string_view type,
    std::string_view member = "") noexcept;

namespace detail {

inline constexpr DomDescription kDomDescriptions[] = {
    // ----- Symbol (base) -------------------------------------------
    {"Symbol", "",
     "Common base of every C++ symbol the corpus exposes. "
     "Concrete symbols (function, record, namespace, etc.) "
     "extend this with their kind-specific fields."},
    {"Symbol", "name",
     "Unqualified name of the symbol, as written in the source."},
    {"Symbol", "id",
     "Stable, base64-encoded identifier used for cross-references "
     "between DOM objects."},
    {"Symbol", "kind",
     "Discriminator selecting the symbol kind (e.g. "
     "`\"function\"`, `\"record\"`). Each concrete symbol type "
     "constrains this field to a single literal value."},
    {"Symbol", "access",
     "Access specifier (`\"public\"`, `\"protected\"`, "
     "`\"private\"`); empty for namespace-scope members."},
    {"Symbol", "extraction",
     "Why the symbol was extracted: `\"regular\"`, "
     "`\"see-below\"`, `\"implementation-defined\"`, or "
     "`\"dependency\"`."},
    {"Symbol", "isCopyFromInherited",
     "True when the symbol is a synthesized copy of an "
     "inherited member (see the `inherit-base-members` "
     "configuration option)."},
    {"Symbol", "parent",
     "Identifier of the enclosing scope, or empty when the "
     "symbol lives in the global namespace."},
    {"Symbol", "doc",
     "Parsed documentation comment attached to the symbol, "
     "or absent when the symbol is undocumented."},
    {"Symbol", "loc",
     "Source location information for the symbol's declaration "
     "and definition."},

    // ----- Symbol synthesized fields -------------------------------
    {"Symbol", "class",
     "Tag set to the literal `\"symbol\"`. Lets templates "
     "discriminate symbol DOM objects from auxiliary types "
     "(`type`, `name`, etc.) without inspecting `kind`."},

    // ----- FunctionSymbol ------------------------------------------
    {"FunctionSymbol", "",
     "A function declaration, including free functions, "
     "member functions, constructors, destructors, and "
     "user-defined operators."},
    {"FunctionSymbol", "params",
     "Function parameter list, in declaration order."},
    {"FunctionSymbol", "returnType",
     "Return type. Absent for constructors and destructors."},
    {"FunctionSymbol", "template",
     "Template head if this is a function template; absent "
     "for non-templates."},
    {"FunctionSymbol", "funcClass",
     "Special-function classification (`\"constructor\"`, "
     "`\"destructor\"`, `\"conversion\"`, etc.) or empty for "
     "ordinary functions."},
    {"FunctionSymbol", "noexcept",
     "Rendered `noexcept` specifier, if any."},
    {"FunctionSymbol", "requires",
     "Trailing `requires`-clause expression as written."},
    {"FunctionSymbol", "isVariadic",
     "True when the function accepts a C-style `...` "
     "argument list."},
    {"FunctionSymbol", "isDefaulted",
     "True when the function is `= default`."},
    {"FunctionSymbol", "isExplicitlyDefaulted",
     "True when the function carries an explicit `= default` "
     "in source (not just an implicit defaulted special "
     "member)."},
    {"FunctionSymbol", "isDeleted",
     "True when the function is `= delete` (whether written "
     "explicitly or implied by language rules)."},
    {"FunctionSymbol", "isDeletedAsWritten",
     "True only when the function is `= delete` as written in "
     "source; distinguishes user-deleted from implicitly "
     "deleted."},
    {"FunctionSymbol", "isNoReturn",
     "True when the function is marked `[[noreturn]]`."},
    {"FunctionSymbol", "hasOverrideAttr",
     "True when the function carries the `override` "
     "specifier."},
    {"FunctionSymbol", "hasTrailingReturn",
     "True when the function uses trailing-return-type "
     "syntax (`auto ... -> T`)."},
    {"FunctionSymbol", "isNodiscard",
     "True when the function is marked `[[nodiscard]]`."},
    {"FunctionSymbol", "isExplicitObjectMemberFunction",
     "True when the function declares an explicit object "
     "parameter (deducing-this member function)."},
    {"FunctionSymbol", "constexpr",
     "Constexpr level: `\"constexpr\"`, `\"consteval\"`, or "
     "empty."},
    {"FunctionSymbol", "overloadedOperator",
     "Overloaded operator name (e.g. `\"operator+\"`) or "
     "empty if not an operator."},
    {"FunctionSymbol", "storageClass",
     "Storage class: `\"static\"`, `\"extern\"`, etc., or "
     "empty."},
    {"FunctionSymbol", "isRecordMethod",
     "True when the function is a member of a class, struct, "
     "or union."},
    {"FunctionSymbol", "isVirtual",
     "True when the function is virtual (whether by `virtual` "
     "keyword or by overriding a virtual function)."},
    {"FunctionSymbol", "isVirtualAsWritten",
     "True only when the `virtual` keyword appears in source."},
    {"FunctionSymbol", "isPure",
     "True when the function is pure virtual (`= 0`)."},
    {"FunctionSymbol", "isConst",
     "True when the function is a `const` member function."},
    {"FunctionSymbol", "isVolatile",
     "True when the function is a `volatile` member "
     "function."},
    {"FunctionSymbol", "isFinal",
     "True when the function is declared `final`."},
    {"FunctionSymbol", "refQualifier",
     "Reference qualifier on the implicit object parameter: "
     "`\"&\"`, `\"&&\"`, or empty."},
    {"FunctionSymbol", "explicit",
     "Rendered `explicit` specifier (with optional condition) "
     "for constructors and conversion functions."},
    {"FunctionSymbol", "attributes",
     "Attributes attached to the declaration."},
    {"FunctionSymbol", "functionObjectImpl",
     "Identifier of the function object this function is the "
     "call-operator implementation of, when the "
     "`auto-function-objects` feature recognized it as such; "
     "empty otherwise."},

    // ----- RecordSymbol --------------------------------------------
    {"RecordSymbol", "",
     "A class, struct, or union declaration."},
    {"RecordSymbol", "keyKind",
     "The introducing keyword: `\"class\"`, `\"struct\"`, "
     "or `\"union\"`."},
    {"RecordSymbol", "isFinal",
     "True if the class is declared `final`."},
    {"RecordSymbol", "bases",
     "Direct base classes, in declaration order."},
    {"RecordSymbol", "template",
     "Template head if this is a class template; absent "
     "for non-templates."},
    {"RecordSymbol", "interface",
     "Per-access summary of the record's members (public, "
     "protected, private, plus aggregated views)."},
    {"RecordSymbol", "isTypeDef",
     "True when the record was introduced by an anonymous "
     "`typedef struct { ... } Name;` declaration."},
    {"RecordSymbol", "isFinalDestructor",
     "True when the record's destructor is declared `final`."},
    {"RecordSymbol", "derived",
     "Identifiers of records that derive from this one and "
     "appear in the corpus."},
    {"RecordSymbol", "friends",
     "List of friend declarations attached to the record."},

    // ----- NamespaceSymbol -----------------------------------------
    {"NamespaceSymbol", "",
     "A namespace declaration. The implicit global namespace "
     "appears as a namespace symbol with an empty name and "
     "no parent."},
    {"NamespaceSymbol", "isInline",
     "True if the namespace is declared `inline`."},
    {"NamespaceSymbol", "isAnonymous",
     "True if the namespace has no name in source."},
    {"NamespaceSymbol", "members",
     "All members of the namespace, grouped into tranches "
     "by kind."},
    {"NamespaceSymbol", "usingDirectives",
     "List of `using namespace` directives found in this "
     "namespace's scope."},

    // ----- EnumSymbol ----------------------------------------------
    {"EnumSymbol", "",
     "An enum declaration (scoped or unscoped)."},
    {"EnumSymbol", "scoped",
     "True for `enum class` / `enum struct`; false for "
     "unscoped enums."},
    {"EnumSymbol", "underlyingType",
     "Underlying integer type, when an explicit one was "
     "specified."},
    {"EnumSymbol", "constants",
     "Identifiers of the enum's constants, in declaration "
     "order."},

    // ----- EnumConstantSymbol --------------------------------------
    {"EnumConstantSymbol", "",
     "A single enumerator within an enum declaration."},
    {"EnumConstantSymbol", "initializer",
     "Initializer expression as written, or empty when the "
     "enumerator takes its implicit value."},

    // ----- TypedefSymbol -------------------------------------------
    {"TypedefSymbol", "",
     "A type alias declaration (`typedef` or `using`)."},
    {"TypedefSymbol", "type",
     "Aliased type."},
    {"TypedefSymbol", "isUsing",
     "True for `using`-style aliases; false for the "
     "`typedef` keyword."},
    {"TypedefSymbol", "template",
     "Template head if this is an alias template; absent "
     "for non-templates."},

    // ----- VariableSymbol ------------------------------------------
    {"VariableSymbol", "",
     "A variable declaration: namespace-scope variable, "
     "static data member, or non-static data member."},
    {"VariableSymbol", "type",
     "Declared type of the variable."},
    {"VariableSymbol", "template",
     "Template head if this is a variable template."},
    {"VariableSymbol", "initializer",
     "Initializer expression as written, or empty when the "
     "variable has no initializer."},
    {"VariableSymbol", "storageClass",
     "Storage class: `\"static\"`, `\"extern\"`, "
     "`\"thread_local\"`, etc., or empty."},
    {"VariableSymbol", "isInline",
     "True when the variable is declared `inline`."},
    {"VariableSymbol", "isConstexpr",
     "True when the variable is declared `constexpr`."},
    {"VariableSymbol", "isConstinit",
     "True when the variable is declared `constinit`."},
    {"VariableSymbol", "isThreadLocal",
     "True when the variable has thread-storage duration."},
    {"VariableSymbol", "isMaybeUnused",
     "True when the variable carries `[[maybe_unused]]`."},
    {"VariableSymbol", "isDeprecated",
     "True when the variable carries `[[deprecated]]`."},
    {"VariableSymbol", "hasNoUniqueAddress",
     "True when the data member carries "
     "`[[no_unique_address]]`."},
    {"VariableSymbol", "isRecordField",
     "True when the variable is a non-static data member of "
     "a class, struct, or union."},
    {"VariableSymbol", "isMutable",
     "True when the data member is declared `mutable`."},
    {"VariableSymbol", "isVariant",
     "True when the data member is part of an anonymous "
     "union."},
    {"VariableSymbol", "isBitfield",
     "True when the data member is a bit-field."},
    {"VariableSymbol", "bitfieldWidth",
     "Bit-field width expression, when the member is a "
     "bit-field."},
    {"VariableSymbol", "attributes",
     "Attributes attached to the declaration."},

    // ----- ConceptSymbol -------------------------------------------
    {"ConceptSymbol", "",
     "A concept declaration."},
    {"ConceptSymbol", "template",
     "Concept's template parameter list."},
    {"ConceptSymbol", "constraint",
     "Constraint expression that defines the concept."},

    // ----- GuideSymbol ---------------------------------------------
    {"GuideSymbol", "",
     "A user-provided class-template deduction guide."},
    {"GuideSymbol", "deduced",
     "Deduced class-template specialization the guide produces."},
    {"GuideSymbol", "template",
     "Template parameters of the deduction guide."},
    {"GuideSymbol", "params",
     "Parameter list used for argument deduction."},
    {"GuideSymbol", "explicit",
     "Rendered `explicit` specifier, if any."},

    // ----- NamespaceAliasSymbol ------------------------------------
    {"NamespaceAliasSymbol", "",
     "A namespace-alias declaration "
     "(`namespace alias = target;`)."},
    {"NamespaceAliasSymbol", "aliasedSymbol",
     "Reference to the namespace this alias refers to."},

    // ----- UsingSymbol ---------------------------------------------
    {"UsingSymbol", "",
     "A `using`-declaration (introduces names from another "
     "scope, including using-enum)."},
    {"UsingSymbol", "class",
     "Kind of `using`-declaration: ordinary, typename, or "
     "enum."},
    {"UsingSymbol", "introducedName",
     "Name introduced into the current scope, qualified by "
     "the source scope."},
    {"UsingSymbol", "shadowDeclarations",
     "Symbols brought into scope by the using-declaration."},

    // ----- OverloadsSymbol -----------------------------------------
    {"OverloadsSymbol", "",
     "An aggregate of overloaded functions sharing a name. "
     "Used to group overload sets in the corpus."},
    {"OverloadsSymbol", "funcClass",
     "Shared special-function classification of the overload "
     "set, when applicable."},
    {"OverloadsSymbol", "overloadedOperator",
     "Shared overloaded-operator name when the overload set "
     "is for an operator."},
    {"OverloadsSymbol", "members",
     "Identifiers of the function declarations belonging to "
     "this overload set."},
    {"OverloadsSymbol", "returnType",
     "Common return type when every overload shares it; "
     "absent otherwise."},

    // ----- Polymorphic union types ---------------------------------
    {"Type", "",
     "A C++ type. The DOM serializes the most-derived kind "
     "(named, pointer, reference, array, function, etc.); see "
     "the `oneOf` entries for the per-kind shape."},
    {"Name", "",
     "A (possibly qualified) name occurring in source, "
     "such as the name of a base class or the type referenced "
     "by a `NamedType`."},
    {"Block", "",
     "A block-level node in a parsed documentation comment "
     "(paragraphs, lists, headings, and command blocks like "
     "`@brief` / `@param` / `@returns`)."},
    {"Inline", "",
     "An inline-level node in a parsed documentation comment "
     "(text, emphasis, code spans, references, etc.)."},
    {"TParam", "",
     "A template parameter of a templated symbol. The DOM "
     "serializes the most-derived kind (type, non-type, "
     "template); see the `oneOf` entries for the per-kind shape."},
    {"TArg", "",
     "A template argument supplied to a template specialization. "
     "The DOM serializes the most-derived kind (type, non-type, "
     "template); see the `oneOf` entries for the per-kind shape."},

    // ----- Polymorphic-base members --------------------------------
    {"Type", "kind",
     "Discriminator selecting the type kind. Each concrete type "
     "constrains this field to a single literal value."},
    {"Type", "isConst",
     "True when the type carries a top-level `const` qualifier."},
    {"Type", "isVolatile",
     "True when the type carries a top-level `volatile` qualifier."},
    {"Type", "isPackExpansion",
     "True when the type appears as the pattern of a pack "
     "expansion (`T...`)."},
    {"Type", "constraints",
     "Constraint expressions associated with the type, such as "
     "those discovered by SFINAE detection around "
     "`std::enable_if_t<..., T>`."},

    {"Name", "kind",
     "Discriminator selecting the name kind. Each concrete name "
     "constrains this field to a single literal value."},
    {"Name", "id",
     "Identifier of the named symbol when it lives in the corpus; "
     "empty when the name refers to something outside it."},
    {"Name", "identifier",
     "Unqualified spelling of the name, as written in the source."},
    {"Name", "prefix",
     "Qualifying prefix (the `A::B::` part of `A::B::Name`); "
     "absent when the name is unqualified."},

    {"TParam", "kind",
     "Discriminator selecting the template-parameter kind. Each "
     "concrete TParam constrains this field to a single literal "
     "value."},
    {"TParam", "name",
     "Parameter name as written; empty for unnamed template "
     "parameters."},
    {"TParam", "default",
     "Default argument expression as written; absent when the "
     "parameter has no default."},
    {"TParam", "isParameterPack",
     "True when the template parameter is a parameter pack "
     "(`typename... Ts`, `int... Ns`, etc.)."},

    {"TArg", "kind",
     "Discriminator selecting the template-argument kind. Each "
     "concrete TArg constrains this field to a single literal "
     "value."},
    {"TArg", "isPackExpansion",
     "True when the argument is a pack expansion (`Args...`)."},

    {"Block", "kind",
     "Discriminator selecting the doc-block kind. Each concrete "
     "block constrains this field to a single literal value."},
    {"Inline", "kind",
     "Discriminator selecting the doc-inline kind. Each concrete "
     "inline constrains this field to a single literal value."},

    {"BlockContainer", "blocks",
     "Block-level children in document order."},
    {"InlineContainer", "children",
     "Inline-level children in document order."},

    // ----- Type variants -------------------------------------------
    {"NamedType", "",
     "A type referred to by name (a class, an enum, a typedef, "
     "or a fundamental type like `int`)."},
    {"NamedType", "name",
     "The name as written, including any qualifications and "
     "template arguments."},
    {"NamedType", "fundamentalType",
     "Fundamental-type tag (`\"int\"`, `\"double\"`, ...) when "
     "the named type is a built-in type; empty otherwise."},

    {"DecltypeType", "",
     "A type expressed as `decltype(expr)`."},
    {"DecltypeType", "operand",
     "The expression inside `decltype(...)`."},

    {"AutoType", "",
     "A placeholder type introduced by `auto` or "
     "`decltype(auto)`."},
    {"AutoType", "keyword",
     "Which placeholder was used: `\"auto\"` or "
     "`\"decltype(auto)\"`."},
    {"AutoType", "constraint",
     "Concept constraint applied to the placeholder, if any."},

    {"LValueReferenceType", "",
     "An lvalue-reference type (`T&`)."},
    {"LValueReferenceType", "pointeeType",
     "The referenced type."},

    {"RValueReferenceType", "",
     "An rvalue-reference type (`T&&`)."},
    {"RValueReferenceType", "pointeeType",
     "The referenced type."},

    {"PointerType", "",
     "A pointer type (`T*`)."},
    {"PointerType", "pointeeType",
     "The pointed-to type."},

    {"MemberPointerType", "",
     "A pointer-to-member type (`T C::*`)."},
    {"MemberPointerType", "parentType",
     "The class type the pointer is a member of."},
    {"MemberPointerType", "pointeeType",
     "The type of the member being pointed to."},

    {"ArrayType", "",
     "An array type (`T[N]`)."},
    {"ArrayType", "elementType",
     "The element type."},
    {"ArrayType", "bounds",
     "Array bound expression as written, or empty for "
     "unknown-bound arrays."},

    {"FunctionType", "",
     "A function type (used for function pointers, "
     "pointers-to-members, etc.)."},
    {"FunctionType", "returnType",
     "The return type."},
    {"FunctionType", "paramTypes",
     "Parameter types, in declaration order."},
    {"FunctionType", "refQualifier",
     "Reference qualifier on the implicit object parameter "
     "(for member-function types): `\"&\"`, `\"&&\"`, or "
     "empty."},
    {"FunctionType", "exceptionSpec",
     "Rendered exception specification, if any."},
    {"FunctionType", "isVariadic",
     "True when the function type accepts a C-style `...` "
     "argument list."},

    // ----- Name variants -------------------------------------------
    {"IdentifierName", "",
     "A plain (possibly qualified) name without template "
     "arguments, e.g. `std::vector` or `MyClass::nested`."},
    {"SpecializationName", "",
     "A template-id: a name applied to template arguments, "
     "e.g. `std::vector<int>` or `Outer<T>::Inner`."},
    {"SpecializationName", "templateArgs",
     "Template arguments applied to the name, in declaration "
     "order."},

    // ----- Param ---------------------------------------------------
    {"Param", "",
     "A function parameter."},
    {"Param", "name",
     "Parameter name as written in the declaration. Empty "
     "for unnamed parameters."},
    {"Param", "type",
     "Declared parameter type."},
    {"Param", "default",
     "Default argument expression as written, or absent if "
     "the parameter has no default."},

    // ----- Location ------------------------------------------------
    {"Location", "",
     "A source location (file path plus line/column)."},
    {"Location", "fullPath",
     "Absolute path to the source file."},
    {"Location", "shortPath",
     "Path relative to the configured source root, suitable "
     "for display."},
    {"Location", "sourcePath",
     "Path relative to the source-root setting, used for "
     "documentation cross-references."},
    {"Location", "lineNumber",
     "1-based line number of the location."},
    {"Location", "columnNumber",
     "1-based column number of the location."},
    {"Location", "documented",
     "True when a doc comment was attached at this location."},

    // ----- Other supporting types ---------------------------------
    {"DocComment", "",
     "Parsed contents of a documentation comment: a brief, "
     "any block-level body, and command groups (`@param`, "
     "`@returns`, `@see`, etc.)."},
    {"DocComment", "brief",
     "First-sentence brief description; absent when none was "
     "extracted."},
    {"DocComment", "document",
     "Free-form body blocks of the comment: every block-level "
     "node not classified into one of the dedicated command "
     "lists below."},
    {"DocComment", "params",
     "Documented function parameters (`@param` commands), in "
     "declaration order."},
    {"DocComment", "tparams",
     "Documented template parameters (`@tparam` commands), in "
     "declaration order."},
    {"DocComment", "returns",
     "Documented return-value description (`@returns` / "
     "`@return` commands)."},
    {"DocComment", "exceptions",
     "Documented exceptions (`@throws` / `@throw` commands)."},
    {"DocComment", "preconditions",
     "Documented preconditions (`@pre` commands)."},
    {"DocComment", "postconditions",
     "Documented postconditions (`@post` commands)."},
    {"DocComment", "sees",
     "See-also references (`@see` commands)."},
    {"DocComment", "related",
     "Related-symbol references (`@related` commands)."},
    {"DocComment", "relates",
     "Symbols this comment attaches to via `@relates`. Drives "
     "the non-member-functions section listed under a class."},

    {"TemplateInfo", "",
     "Template metadata attached to a templated symbol: "
     "parameters, arguments (for specializations), and a "
     "reference to the primary template."},
    {"TemplateInfo", "params",
     "Template parameter list, in declaration order."},
    {"TemplateInfo", "args",
     "Template arguments applied to the primary template, in "
     "declaration order. Present on (partial) specializations "
     "and instantiations; empty for the primary template "
     "itself."},
    {"TemplateInfo", "primary",
     "Identifier of the primary template this entry "
     "specializes; empty when this is itself the primary."},
    {"TemplateInfo", "requires",
     "Trailing `requires`-clause expression as written; empty "
     "when the template has no constraints."},

    {"SourceInfo", "",
     "Source-location metadata for a symbol: every declaration "
     "site plus the definition site, when one exists."},
    {"SourceInfo", "loc",
     "Locations of the symbol's declarations (one entry per "
     "source declaration)."},
    {"SourceInfo", "defLoc",
     "Location of the symbol's definition; absent when no "
     "definition was extracted."},

    {"BaseInfo", "",
     "A base-class clause of a class or struct declaration."},
    {"BaseInfo", "type",
     "Base type as written in the inheritance clause (a class "
     "or template specialization)."},
    {"BaseInfo", "access",
     "Access specifier of the base (`\"public\"`, "
     "`\"protected\"`, or `\"private\"`)."},
    {"BaseInfo", "isVirtual",
     "True when the base is inherited virtually."},

    {"FriendInfo", "",
     "A `friend` declaration appearing inside a class or "
     "struct."},
    {"FriendInfo", "id",
     "Identifier of the symbol befriended when it lives in the "
     "corpus; empty when the friend points outside it."},
    {"FriendInfo", "type",
     "Type befriended, used when the friend declaration names a "
     "type rather than a corpus symbol."},

    // ----- TArg variants -------------------------------------------
    {"TypeTArg", "",
     "A template argument that is a type, e.g. `int` in "
     "`std::vector<int>`."},
    {"TypeTArg", "type",
     "The argument type."},

    {"ConstantTArg", "",
     "A template argument that is a non-type value, e.g. "
     "`42` in `std::array<int, 42>`."},
    {"ConstantTArg", "value",
     "The argument expression as written."},

    {"TemplateTArg", "",
     "A template argument that is itself a template, used "
     "for template-template parameters."},
    {"TemplateTArg", "template",
     "Identifier of the referenced template."},
    {"TemplateTArg", "name",
     "Name of the template as written."},

    // ----- TParam variants -----------------------------------------
    {"TypeTParam", "",
     "A type template parameter (`template <typename T>` or "
     "`template <class T>`)."},
    {"TypeTParam", "keyKind",
     "Introducing keyword: `\"typename\"` or `\"class\"`."},
    {"TypeTParam", "constraint",
     "Concept constraint applied to the parameter, if any."},

    {"ConstantTParam", "",
     "A non-type template parameter "
     "(`template <int N>`, `template <auto V>`, ...)."},
    {"ConstantTParam", "type",
     "Declared type of the parameter."},

    {"TemplateTParam", "",
     "A template template parameter "
     "(`template <template <typename> class C>`)."},
    {"TemplateTParam", "params",
     "Inner template parameter list of the parameter."},

    // ----- Block variants ------------------------------------------
    {"AdmonitionBlock", "",
     "An admonition (note, warning, etc.) container "
     "introduced by `@note`, `@warning`, and similar "
     "commands."},
    {"AdmonitionBlock", "admonish",
     "Admonition kind: `\"note\"`, `\"tip\"`, "
     "`\"important\"`, `\"caution\"`, or `\"warning\"`."},

    {"BriefBlock", "",
     "The brief description of a symbol, sourced from a "
     "`@brief` command, `@copybrief` reference, or the first "
     "sentence of the doc comment."},
    {"BriefBlock", "copiedFrom",
     "Identifier of the symbol the brief was copied from "
     "via `@copybrief`, or empty."},

    {"CodeBlock", "",
     "A fenced code block in the doc comment."},
    {"CodeBlock", "literal",
     "Verbatim contents of the code block."},
    {"CodeBlock", "info",
     "Info string after the opening fence (typically the "
     "language identifier, e.g. `\"cpp\"`)."},

    {"DefinitionListBlock", "",
     "A definition list (terms with descriptions)."},
    {"DefinitionListBlock", "items",
     "Definition-list items, each pairing a term with its "
     "description."},

    {"FootnoteDefinitionBlock", "",
     "A footnote definition referenced by a "
     "`FootnoteReferenceInline` elsewhere in the comment."},
    {"FootnoteDefinitionBlock", "label",
     "Label that footnote references use to point at this "
     "definition."},

    {"HeadingBlock", "",
     "A heading within the doc comment."},
    {"HeadingBlock", "level",
     "Heading depth (1 for top-level, 2 for second-level, "
     "and so on)."},

    {"ListBlock", "",
     "An ordered or unordered list."},
    {"ListBlock", "items",
     "List items, each holding a sequence of blocks."},
    {"ListBlock", "listKind",
     "Whether the list is ordered or unordered."},

    {"MathBlock", "",
     "A display-math block."},
    {"MathBlock", "literal",
     "Verbatim TeX/LaTeX source for the formula."},

    {"ParagraphBlock", "",
     "A paragraph: a sequence of inline elements terminated "
     "by a blank line."},

    {"ParamBlock", "",
     "Documentation of a function parameter, sourced from a "
     "`@param` command."},
    {"ParamBlock", "name",
     "Name of the documented parameter (matches a parameter "
     "of the enclosing function)."},
    {"ParamBlock", "direction",
     "Parameter direction (`\"in\"`, `\"out\"`, "
     "`\"inout\"`), or empty when unspecified."},

    {"PostconditionBlock", "",
     "A `@post` clause describing a function postcondition."},

    {"PreconditionBlock", "",
     "A `@pre` clause describing a function precondition."},

    {"QuoteBlock", "",
     "A block-level quotation, holding nested blocks."},

    {"ReturnsBlock", "",
     "Documentation of the function's return value, sourced "
     "from a `@returns` / `@return` command."},

    {"SeeBlock", "",
     "A `@see` cross-reference clause."},

    {"TParamBlock", "",
     "Documentation of a template parameter, sourced from a "
     "`@tparam` command."},
    {"TParamBlock", "name",
     "Name of the documented template parameter."},

    {"TableBlock", "",
     "A table with header and body rows."},
    {"TableBlock", "alignments",
     "Per-column alignment for the table."},
    {"TableBlock", "items",
     "Rows of the table, each a sequence of cells."},

    {"ThematicBreakBlock", "",
     "A horizontal rule (`---`) separating sections."},

    {"ThrowsBlock", "",
     "Documentation of an exception thrown by the function, "
     "sourced from a `@throws` / `@throw` / `@exception` "
     "command."},
    {"ThrowsBlock", "exception",
     "Name of the exception type the function may throw."},

    // ----- Inline variants -----------------------------------------
    {"CodeInline", "",
     "An inline code span (`` `like this` ``)."},

    {"CopyDetailsInline", "",
     "A pending `@copydetails` reference. Resolved during "
     "doc-comment finalization; surviving instances indicate "
     "an unresolved reference."},
    {"CopyDetailsInline", "string",
     "The reference string as written after `@copydetails`."},
    {"CopyDetailsInline", "id",
     "Identifier of the referenced symbol once resolved, or "
     "empty if the reference could not be resolved."},

    {"EmphInline", "",
     "Emphasized inline text (typically rendered italic)."},

    {"FootnoteReferenceInline", "",
     "An inline reference pointing at a "
     "`FootnoteDefinitionBlock` with the matching label."},
    {"FootnoteReferenceInline", "label",
     "Label of the footnote being referenced."},

    {"HighlightInline", "",
     "Inline text rendered with highlight (e.g. `<mark>`) "
     "styling."},

    {"ImageInline", "",
     "An inline image."},
    {"ImageInline", "src",
     "Image source URL or path."},
    {"ImageInline", "alt",
     "Alternative text for the image."},

    {"LineBreakInline", "",
     "A hard line break within a paragraph "
     "(`<br>`-equivalent)."},

    {"LinkInline", "",
     "A hyperlink whose visible text is the contained "
     "inlines."},
    {"LinkInline", "href",
     "Target URL of the link."},

    {"MathInline", "",
     "An inline math span."},
    {"MathInline", "literal",
     "Verbatim TeX/LaTeX source for the formula."},

    {"ReferenceInline", "",
     "A cross-reference to a symbol in the corpus, produced "
     "by `@ref`, `@p`, or implicit name lookup."},
    {"ReferenceInline", "literal",
     "The reference text as written in the doc comment."},
    {"ReferenceInline", "id",
     "Identifier of the resolved target, or empty when the "
     "reference could not be resolved."},

    {"SoftBreakInline", "",
     "A soft line break within a paragraph (rendered as "
     "whitespace by most generators)."},

    {"StrikethroughInline", "",
     "Inline text with strikethrough styling."},

    {"StrongInline", "",
     "Strongly emphasized inline text (typically rendered "
     "bold)."},

    {"SubscriptInline", "",
     "Inline text rendered as subscript."},

    {"SuperscriptInline", "",
     "Inline text rendered as superscript."},

    {"TextInline", "",
     "A run of plain text within a paragraph or other "
     "inline container."},
    {"TextInline", "literal",
     "Verbatim text content."},
};

} // namespace detail

inline std::string_view
findDomDescription(
    std::string_view const type,
    std::string_view const member) noexcept
{
    for (auto const& entry : detail::kDomDescriptions)
    {
        if (entry.type == type && entry.member == member)
        {
            return entry.text;
        }
    }
    MRDOCS_UNREACHABLE();
}

} // namespace mrdocs::schema

#endif // MRDOCS_LIB_SCHEMAS_DOMDESCRIPTIONS_HPP
