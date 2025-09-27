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

#include <lib/Gen/xml/XMLTags.hpp>
#include <mrdocs/Metadata/Name.hpp>
#include <mrdocs/Metadata/Symbol/Function.hpp>
#include <mrdocs/Metadata/TParam.hpp>
#include <mrdocs/Metadata/Type.hpp>

/*
    This file holds the business logic for transforming
    metadata into XML tags. Constants here are reflected
    in the MRDOCS DTD XML schema.
*/

namespace mrdocs::xml {

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
constexpr auto relatedTagName        = "related";
constexpr auto relatesTagName        = "relates";

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
writeType(Type const& I,
    XMLTags& tags,
    std::string_view type_tag = "type")
{
    visit(I, [&]<typename T>(T const& t)
        {
            Attributes attrs = {
                { "class", toString(T::kind_id),
                    T::kind_id != TypeKind::Named },
                { "is-pack", "1", t.IsPackExpansion }
            };

            // KRYSTIAN FIXME: parent should be a type itself
            if constexpr(requires { t.ParentType; })
            {
                MRDOCS_ASSERT(!t.ParentType.valueless_after_move());
                attrs.push({ "parent", toString(*t.ParentType) });
            }

            if constexpr(T::isNamed())
            {
                MRDOCS_ASSERT(!t.Name.valueless_after_move());
                attrs.push({t.Name->id});
                attrs.push({"name", toString(*t.Name)});
            }

            std::string cvQualifiers;
            if (t.IsConst)
            {
                cvQualifiers += "const";
            }
            if (t.IsVolatile)
            {
                if (!cvQualifiers.empty())
                {
                    cvQualifiers += ' ';
                }
                cvQualifiers += "volatile";
            }
            if (!cvQualifiers.empty())
            {
                attrs.push({"cv-qualifiers", cvQualifiers});
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
                AutoType const& at = t;
                attrs.push({"keyword", toString(at.Keyword)});
                if(at.Constraint)
                {
                    MRDOCS_ASSERT(!at.Constraint->valueless_after_move());
                    attrs.push({"constraint", toString(**t.Constraint)});
                }
            }

            if constexpr(T::isFunction())
            {
                if(t.IsVariadic)
                    attrs.push({"is-variadic", "1"});

                if(t.RefQualifier != ReferenceKind::None)
                    attrs.push({"ref-qualifier", toString(t.RefQualifier)});

                // KRYSTIAN TODO: Type should use ExceptionInfo!
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
                Polymorphic<Type> const& pointee = t.PointeeType;
                MRDOCS_ASSERT(!pointee.valueless_after_move());
                writeType(*t.PointeeType, tags, "pointee-type");
            }

            if constexpr(T::isArray())
            {
                ArrayType const& at = t;
                MRDOCS_ASSERT(!at.ElementType.valueless_after_move());
                writeType(*t.ElementType, tags, "element-type");
            }

            if constexpr(T::isFunction())
            {
                FunctionType const& ft = t;
                MRDOCS_ASSERT(!ft.ReturnType.valueless_after_move());
                writeType(*t.ReturnType, tags, "return-type");
                for (auto const& p: t.ParamTypes)
                {
                    writeType(*p, tags, "param-type");
                }
            }
            tags.close(type_tag);
        });
}


inline
void
writeType(
    Name const& I,
    XMLTags& tags,
    std::string_view type_tag = "type")
{
    Attributes attrs = {
        { "class", toString(TypeKind::Named), false },
        { "is-pack", "1", false }
    };

    attrs.push({I.id});
    attrs.push({"name", I.Identifier});

    tags.write(type_tag, {}, std::move(attrs));
}

inline
void
writeType(
    Polymorphic<Type> const& type,
    XMLTags& tags)
{
    MRDOCS_ASSERT(!type.valueless_after_move());
    writeType(*type, tags);
}

inline
void
writeType(
    Optional<Polymorphic<Type>> const& type,
    XMLTags& tags)
{
    if (type)
    {
        writeType(*type, tags);
    }
}


inline void writeReturnType(Type const& I, XMLTags& tags)
{
    // KRYSTIAN NOTE: we don't *have* to do this...
    if (toString(I) == "void")
    {
        return;
    }
    tags.open(returnTagName);
    writeType(I, tags);
    tags.close(returnTagName);
}

inline void writeParam(Param const& P, XMLTags& tags)
{
    tags.open(paramTagName, {
        { "name", P.Name.has_value() ? *P.Name : "", P.Name.has_value() },
        { "default", P.Default.has_value() ? *P.Default : "", P.Default.has_value() },
        });
    writeType(*P.Type, tags);
    tags.close(paramTagName);
}

inline void writeTemplateParam(TParam const& I, XMLTags& tags)
{
    visit(I, [&]<typename T>(T const& P) {
        TParam const & tp = P;
        Attributes attrs = {
            { "name", tp.Name, !tp.Name.empty() },
            { "class", toString(T::kind_id) }
        };

        if constexpr (T::isConstant())
        {
            ConstantTParam const& nt = P;
            MRDOCS_ASSERT(!nt.Type.valueless_after_move());
            attrs.push({ "type", toString(*nt.Type) });
        }

        if (tp.Default)
        {
            MRDOCS_ASSERT(!tp.Default->valueless_after_move());
            attrs.push({ "default", toString(**P.Default) });
        }

        if constexpr (T::isTemplate())
        {
            tags.open(tparamTagName, std::move(attrs));
            for (auto const& tparam: P.Params)
            {
                writeTemplateParam(*tparam, tags);
            }
            tags.close(tparamTagName);
        } else
        {
            tags.write(tparamTagName, {}, std::move(attrs));
        }
    });
}

inline void writeTemplateArg(TArg const& I, XMLTags& tags)
{
    visit(I, [&]<typename T>(T const& A) {
        Attributes attrs = {
            { "class", toString(T::kind_id) }
        };

        if constexpr (T::isType())
        {
            TypeTArg const& at = A;
            MRDOCS_ASSERT(!at.Type.valueless_after_move());
            attrs.push({ "type", toString(*A.Type) });
        }
        if constexpr (T::isConstant())
        {
            attrs.push({ "value", A.Value.Written });
        }
        if constexpr (T::isTemplate())
        {
            attrs.push({ "name", A.Name });
            attrs.push({ A.Template });
        }

        tags.write(targTagName, {}, std::move(attrs));
    });
}

/** Return the xml tag name for the Info.
*/
std::string
getTagName(Symbol const& I) noexcept;

} // mrdocs::xml

#endif
