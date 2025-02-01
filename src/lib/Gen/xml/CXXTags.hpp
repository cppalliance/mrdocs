//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#ifndef MRDOCS_LIB_GEN_XML_CXXTAGS_HPP
#define MRDOCS_LIB_GEN_XML_CXXTAGS_HPP

#include "XMLTags.hpp"
#include <mrdocs/Metadata/Info/Function.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Type.hpp>

/*
    This file holds the business logic for transforming
    metadata into XML tags. Constants here are reflected
    in the MRDOCS DTD XML schema.
*/

namespace clang {
namespace mrdocs {
namespace xml {

#define INFO(camelName, LowerName) \
constexpr auto camelName##TagName = #LowerName;
#include <mrdocs/Metadata/InfoNodesCamelAndLower.inc>

constexpr auto accessTagName         = "access";
constexpr auto attributeTagName      = "attr";
constexpr auto baseTagName           = "base";
constexpr auto bitfieldTagName       = "bitfield";
constexpr auto classTagName          = "class";
constexpr auto dataMemberTagName     = "field";
constexpr auto javadocTagName        = "doc";
constexpr auto paramTagName          = "param";
constexpr auto returnTagName         = "return";
constexpr auto deducedTagName        = "deduced";
constexpr auto structTagName         = "struct";
constexpr auto targTagName           = "targ";
constexpr auto templateTagName       = "template";
constexpr auto tparamTagName         = "tparam";
constexpr auto unionTagName          = "union";
constexpr auto varTagName            = "variable";

inline dom::String getNameForValue(...)
{
    return "";
}

inline dom::String getNameForValue(ConstexprKind kind)
{
    return toString(kind);
}

inline dom::String getNameForValue(NoexceptKind kind)
{
    return toString(kind);
}

inline dom::String getNameForValue(StorageClassKind kind)
{
    return toString(kind);
}

inline dom::String getNameForValue(ReferenceKind kind)
{
    return toString(kind);
}

inline dom::String getNameForValue(ExplicitKind kind)
{
    return toString(kind);
}

inline dom::String getNameForValue(OperatorKind kind)
{
    return getSafeOperatorName(kind);
}

//------------------------------------------------

template<typename T>
void
writeAttr(
    T value,
    std::string_view name,
    XMLTags& tags)
{
    using ValueType = std::remove_cvref_t<T>;

    if constexpr(std::is_enum_v<ValueType>)
    {
        if(auto converted = to_underlying(value))
        {
            tags.write(attributeTagName, {}, {
                { "id", name },
                { "name", getNameForValue(value) },
                { "value", std::to_string(converted) } });
        }
    }
    else if constexpr(std::same_as<ValueType, bool>)
    {
        if(value)
            tags.write(attributeTagName, {}, { { "id", name } });
    }
    else
    {
        tags.write(attributeTagName, {}, {
                { "id", name },
                { "value", std::to_string(value) } });
    }
}

inline
void
writeTemplateArg(TArg const& I, XMLTags& tags);

inline
void
writeType(
    TypeInfo const& I,
    XMLTags& tags,
    std::string_view type_tag = "type")
{
    visit(I, [&]<typename T>(
        T const& t)
        {
            Attributes attrs = {
                { "class", toString(T::kind_id),
                    T::kind_id != TypeKind::Named },
                { "is-pack", "1", t.IsPackExpansion }
            };

            // KRYSTIAN FIXME: parent should is a type itself
            if constexpr(requires { t.ParentType; })
            {
                if(t.ParentType)
                    attrs.push({"parent", toString(*t.ParentType)});
            }

            if constexpr(T::isNamed())
            {
                if(t.Name)
                {
                    attrs.push({t.Name->id});
                    attrs.push({"name", toString(*t.Name)});
                }
            }

            if constexpr(requires { t.CVQualifiers; })
            {
                if(t.CVQualifiers != QualifierKind::None)
                    attrs.push({"cv-qualifiers", toString(t.CVQualifiers)});
            }

            if constexpr(T::isArray())
            {
                std::string bounds = t.Bounds.Value ?
                    std::to_string(*t.Bounds.Value) :
                    t.Bounds.Written;
                if(! bounds.empty())
                    attrs.push({"bounds", bounds});
            }

            if constexpr(T::isDecltype())
            {
                attrs.push({"operand", t.Operand.Written});
            }

            if constexpr(T::isAuto())
            {
                attrs.push({"keyword", toString(t.Keyword)});
                if(t.Constraint)
                    attrs.push({"constraint", toString(*t.Constraint)});
            }

            if constexpr(T::isFunction())
            {
                if(t.IsVariadic)
                    attrs.push({"is-variadic", "1"});

                if(t.RefQualifier != ReferenceKind::None)
                    attrs.push({"ref-qualifier", toString(t.RefQualifier)});

                // KRYSTIAN TODO: TypeInfo should use ExceptionInfo!
                if(auto spec = toString(t.ExceptionSpec); ! spec.empty())
                    attrs.push({"exception-spec", spec});
            }

            // ----------------------------------------------------------------

            // no nested types; write as self closing tag
            if constexpr(T::isNamed())
            {
                tags.write(type_tag, {}, std::move(attrs));
                return;
            }

            tags.open(type_tag, std::move(attrs));

            if constexpr(requires { t.PointeeType; })
            {
                writeType(*t.PointeeType, tags, "pointee-type");
            }

            if constexpr(T::isArray())
            {
                writeType(*t.ElementType, tags, "element-type");
            }

            if constexpr(T::isFunction())
            {
                writeType(*t.ReturnType, tags, "return-type");
                for(auto const& p : t.ParamTypes)
                    writeType(*p, tags, "param-type");
            }

            tags.close(type_tag);
        });
}

inline
void
writeType(
    Polymorphic<TypeInfo> const& type,
    XMLTags& tags)
{
    if(! type)
        return;
    writeType(*type, tags);
}

inline void writeReturnType(TypeInfo const& I, XMLTags& tags)
{
    // KRYSTIAN NOTE: we don't *have* to do this...
    if(toString(I) == "void")
        return;
    tags.open(returnTagName);
    writeType(I, tags);
    tags.close(returnTagName);
}

inline void writeParam(Param const& P, XMLTags& tags)
{
    tags.open(paramTagName, {
        { "name", P.Name, ! P.Name.empty() },
        { "default", P.Default, ! P.Default.empty() },
        });
    writeType(*P.Type, tags);
    tags.close(paramTagName);
}

inline void writeTemplateParam(TParam const& I, XMLTags& tags)
{
    visit(I, [&]<typename T>(T const& P)
        {
            Attributes attrs = {
                {"name", P.Name, ! P.Name.empty()},
                {"class", toString(T::kind_id)}
            };

            if constexpr(T::isNonType())
                attrs.push({"type", toString(*P.Type)});

            if(P.Default)
                attrs.push({"default", toString(*P.Default)});

            if constexpr(T::isTemplate())
            {
                tags.open(tparamTagName,
                    std::move(attrs));
                for(const auto& tparam : P.Params)
                    writeTemplateParam(*tparam, tags);
                tags.close(tparamTagName);
            }
            else
            {
                tags.write(tparamTagName, {},
                    std::move(attrs));
            }
        });
}

inline void writeTemplateArg(const TArg& I, XMLTags& tags)
{
    visit(I, [&]<typename T>(const T& A)
        {
            Attributes attrs = {
                {"class", toString(T::kind_id)}
            };

            if constexpr(T::isType())
            {
                attrs.push({"type", toString(*A.Type)});
            }
            if constexpr(T::isNonType())
            {
                attrs.push({"value", A.Value.Written});
            }
            if constexpr(T::isTemplate())
            {
                attrs.push({"name", A.Name});
                attrs.push({A.Template});
            }

            tags.write(targTagName, {},
                std::move(attrs));
        });
}

/** Return the xml tag name for the Info.
*/
llvm::StringRef
getTagName(Info const& I) noexcept;

} // xml
} // mrdocs
} // clang

#endif
