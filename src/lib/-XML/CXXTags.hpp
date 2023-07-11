//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#ifndef MRDOX_LIB_XML_CXXTAGS_HPP
#define MRDOX_LIB_XML_CXXTAGS_HPP

#include "XMLTags.hpp"
#include <mrdox/Metadata/Function.hpp>
#include <mrdox/Metadata/Record.hpp>
#include <mrdox/Metadata/Type.hpp>
#include <mrdox/Metadata/Variable.hpp>
#include <mrdox/Platform.hpp>

/*
    This file holds the business logic for transforming
    metadata into XML tags. Constants here are reflected
    in the MRDOX DTD XML schema.
*/

namespace clang {
namespace mrdox {
namespace xml {

constexpr auto accessTagName         = "access";
constexpr auto aliasTagName          = "alias";
constexpr auto attributeTagName      = "attr";
constexpr auto baseTagName           = "base";
constexpr auto bitfieldTagName       = "bitfield";
constexpr auto classTagName          = "class";
constexpr auto dataMemberTagName     = "field";
constexpr auto javadocTagName        = "doc";
constexpr auto enumTagName           = "enum";
constexpr auto friendTagName         = "friend";
constexpr auto functionTagName       = "function";
constexpr auto namespaceTagName      = "namespace";
constexpr auto paramTagName          = "param";
constexpr auto returnTagName         = "return";
constexpr auto structTagName         = "struct";
constexpr auto specializationTagName = "specialization";
constexpr auto targTagName           = "targ";
constexpr auto templateTagName       = "template";
constexpr auto tparamTagName         = "tparam";
constexpr auto typedefTagName        = "typedef";
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

template<class BitFieldUnion>
struct BitFieldWriter
{
    BitFieldUnion field;
    XMLTags& tags;

    BitFieldWriter(BitFieldUnion field, XMLTags& tags)
        : field(field), tags(tags)
    {
    }

    template<unsigned char Offset,
             unsigned char Size,
             typename T>
    void write(BitField<Offset, Size, T> BitFieldUnion :: * member,
                          const char * idName)
    {
        const auto v = (field.*member).get();

        if constexpr (std::is_enum_v<T>)
        {
            if(static_cast<std::uint32_t>(v) == 0)
                return;
            tags.write(attributeTagName, {}, {
                    { "id", idName },
                    { "name", getNameForValue(v) },
                    { "value", std::to_string(static_cast<
                                      std::underlying_type_t<T>>(v)) } });
        }
        else if constexpr (BitField<Offset, Size, T>::size == 1u)
        {
            if (v)
                tags.write(attributeTagName, {}, { { "id", idName } });
        }
        else
            tags.write(attributeTagName, {}, {
                    { "id", idName },
                    { "value", std::to_string(v) } });
    }
};

inline void write(RecFlags0 const& bits, XMLTags& tags)
{
    BitFieldWriter<RecFlags0> fw(bits, tags);
    fw.write(&RecFlags0::isFinal, "is-final");
    fw.write(&RecFlags0::isFinalDestructor, "is-final-dtor");
}

inline void write(FnFlags0 const& bits, XMLTags& tags)
{
    BitFieldWriter<FnFlags0> fw(bits, tags);
    fw.write(&FnFlags0::isVariadic,            "is-variadic");
    fw.write(&FnFlags0::isVirtualAsWritten,    "is-virtual-as-written");
    fw.write(&FnFlags0::isPure,                "is-pure");
    fw.write(&FnFlags0::isDefaulted,           "is-defaulted");
    fw.write(&FnFlags0::isExplicitlyDefaulted, "is-explicitly-defaulted");
    fw.write(&FnFlags0::isDeleted,             "is-deleted");
    fw.write(&FnFlags0::isDeletedAsWritten,    "is-deleted-as-written");
    fw.write(&FnFlags0::isNoReturn,            "is-no-return");
    fw.write(&FnFlags0::hasOverrideAttr,       "has-override");
    fw.write(&FnFlags0::hasTrailingReturn,     "has-trailing-return");
    fw.write(&FnFlags0::constexprKind,         "constexpr-kind");
    fw.write(&FnFlags0::exceptionSpec    ,     "exception-spec");
    fw.write(&FnFlags0::overloadedOperator,    "operator");
    fw.write(&FnFlags0::storageClass,          "storage-class");
    fw.write(&FnFlags0::isConst,               "is-const");
    fw.write(&FnFlags0::isVolatile,            "is-volatile");
    fw.write(&FnFlags0::refQualifier,          "ref-qualifier");
}

inline void write(FnFlags1 const& bits, XMLTags& tags)
{
    BitFieldWriter<FnFlags1> fw(bits, tags);

    fw.write(&FnFlags1::explicitSpec,      "explicit-spec");
    fw.write(&FnFlags1::isNodiscard,       "nodiscard");
}


inline void write(FieldFlags const& bits, XMLTags& tags)
{
    BitFieldWriter<FieldFlags> fw(bits, tags);

    fw.write(&FieldFlags::isMaybeUnused, "maybe-unused");
    fw.write(&FieldFlags::isDeprecated, "deprecated");
    fw.write(&FieldFlags::hasNoUniqueAddress, "no-unique-address");
}


inline void write(VariableFlags0 const& bits, XMLTags& tags)
{
    BitFieldWriter<VariableFlags0> fw(bits, tags);
    fw.write(&VariableFlags0::storageClass, "storage-class");
    fw.write(&VariableFlags0::constexprKind, "constexpr-kind");
    fw.write(&VariableFlags0::isConstinit, "is-constinit");
    fw.write(&VariableFlags0::isThreadLocal, "is-thread-local");
}

inline
void
writeTemplateArg(const TArg& I, XMLTags& tags);

inline
void
writeType(
    TypeInfo const& I,
    XMLTags& tags,
    std::string_view type_tag = "type")
{
    visit(I, [&]<typename T>(
        const T& t)
        {
            Attributes attrs = {
                { "class", toString(T::kind_id),
                    T::kind_id != TypeKind::Builtin }
            };

            if constexpr(requires { t.id; })
            {
                attrs.push({t.id});
            }

            // KRYSTIAN FIXME: parent should is a type itself
            if constexpr(requires { t.ParentType; })
            {
                if(t.ParentType)
                    attrs.push({"parent", toString(*t.ParentType)});
            }

            if constexpr(requires { t.Name; })
            {
                attrs.push({"name", t.Name});
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

            if constexpr(T::isFunction())
            {
                if(t.RefQualifier != ReferenceKind::None)
                    attrs.push({"ref-qualifier", toString(t.RefQualifier)});

                if(t.ExceptionSpec != NoexceptKind::None)
                    attrs.push({"exception-spec", toString(t.ExceptionSpec)});
            }

            // ----------------------------------------------------------------

            // no nested types; write as self closing tag
            if constexpr(T::isBuiltin() || T::isTag())
            {
                tags.write(type_tag, {}, std::move(attrs));
                return;
            }

            tags.open(type_tag, std::move(attrs));

            if constexpr(T::isSpecialization())
            {
                for(const auto& targ : t.TemplateArgs)
                    writeTemplateArg(targ, tags);
            }

            if constexpr(requires { t.PointeeType; })
            {
                writeType(*t.PointeeType, tags, "pointee-type");
            }

            if constexpr(T::isPack())
            {
                writeType(*t.PatternType, tags, "pattern-type");
            }

            if constexpr(T::isArray())
            {
                writeType(*t.ElementType, tags, "element-type");
            }

            if constexpr(T::isFunction())
            {
                writeType(*t.ReturnType, tags, "return-type");
                for(const auto& p : t.ParamTypes)
                    writeType(*p, tags, "param-type");
            }

            tags.close(type_tag);
        });
}

inline
void
writeType(
    const std::unique_ptr<TypeInfo>& type,
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

inline void writeTemplateParam(const TParam& I, XMLTags& tags)
{
    switch(I.Kind)
    {
    case TParamKind::Type:
    {
        const auto& t = I.get<TypeTParam>();

        std::string default_val;
        if(t.Default)
            default_val = toString(*t.Default);

        tags.write(tparamTagName, {}, {
            { "name", I.Name, ! I.Name.empty() },
            { "class", "type" },
            { "default", default_val, ! default_val.empty() }
        });
        break;
    }
    case TParamKind::NonType:
    {
        const auto& t = I.get<NonTypeTParam>();
        std::string_view default_val;
        if(t.Default)
            default_val = t.Default.value();

        tags.write(tparamTagName, {}, {
            { "name", I.Name, ! I.Name.empty() },
            { "class", "non-type" },
            // KRYSTIAN FIXME: we can use writeType if really care
            { "type", toString(*t.Type) },
            { "default", default_val, ! default_val.empty() }
        });
        break;
    }
    case TParamKind::Template:
    {
        const auto& t = I.get<TemplateTParam>();
        std::string_view default_val;
        if(t.Default)
            default_val = t.Default.value();
        tags.open(tparamTagName, {
            { "name", I.Name, ! I.Name.empty() },
            { "class", "template" },
            { "default", default_val, ! default_val.empty() }
        });
        for(const auto& P : t.Params)
            writeTemplateParam(P, tags);
        tags.close(tparamTagName);
        break;
    }
    default:
    {
        tags.write(tparamTagName, {}, {
            { "name", I.Name, ! I.Name.empty() }
        });
        break;
    }
    }
}

inline void writeTemplateArg(const TArg& I, XMLTags& tags)
{
    tags.write(targTagName, {}, {
        { "value", I.Value }
    });
}

/** Return the xml tag name for the Info.
*/
llvm::StringRef
getTagName(Info const& I) noexcept;

} // xml
} // mrdox
} // clang

#endif
