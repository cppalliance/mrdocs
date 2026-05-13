//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Schemas/DomSchemaWriter.hpp>
#include <test_suite/test_suite.hpp>
#include <cstddef>

namespace mrdocs {
namespace {

// Helpers to navigate the schema dom::Value tree.

dom::Object
defs(dom::Value const& schema)
{
    return schema.getObject().get("$defs").getObject();
}

dom::Object
props(dom::Object const& obj, std::string_view defName)
{
    return obj.get(defName).getObject()
              .get("properties").getObject();
}

dom::Array
oneOfArray(dom::Object const& obj, std::string_view defName)
{
    return obj.get(defName).getObject()
              .get("oneOf").getArray();
}

dom::Array
requiredArray(dom::Object const& obj, std::string_view defName)
{
    return obj.get(defName).getObject()
              .get("required").getArray();
}

bool
arrayContains(dom::Array const& arr, std::string_view value)
{
    for (std::size_t i = 0; i < arr.size(); ++i)
    {
        if (arr.at(i).getString() == value)
            return true;
    }
    return false;
}

// ------------------------------------------------------------------

struct DomSchemaWriterTest
{
    // -- Top-level structure --------------------------------------

    void test_top_level()
    {
        dom::Value schema = schema::buildDomJsonSchema();
        dom::Object root = schema.getObject();

        BOOST_TEST(root.exists("$schema"));
        BOOST_TEST(root.exists("title"));
        BOOST_TEST(root.exists("$defs"));
        BOOST_TEST(root.exists("oneOf"));

        // The top-level oneOf lists all 12 symbol kinds.
        BOOST_TEST(root.get("oneOf").getArray().size() == 12);
    }

    // -- Symbol coverage ------------------------------------------

    void test_all_symbols_in_defs()
    {
        dom::Value schema = schema::buildDomJsonSchema();
        dom::Object d = defs(schema);

        BOOST_TEST(d.exists("NamespaceSymbol"));
        BOOST_TEST(d.exists("RecordSymbol"));
        BOOST_TEST(d.exists("FunctionSymbol"));
        BOOST_TEST(d.exists("OverloadsSymbol"));
        BOOST_TEST(d.exists("EnumSymbol"));
        BOOST_TEST(d.exists("EnumConstantSymbol"));
        BOOST_TEST(d.exists("TypedefSymbol"));
        BOOST_TEST(d.exists("VariableSymbol"));
        BOOST_TEST(d.exists("GuideSymbol"));
        BOOST_TEST(d.exists("NamespaceAliasSymbol"));
        BOOST_TEST(d.exists("UsingSymbol"));
        BOOST_TEST(d.exists("ConceptSymbol"));
    }

    // -- Polymorphic oneOf ----------------------------------------

    void test_polymorphic_variants()
    {
        dom::Value schema = schema::buildDomJsonSchema();
        dom::Object d = defs(schema);

        BOOST_TEST(oneOfArray(d, "Type").size() == 9);
        BOOST_TEST(oneOfArray(d, "Name").size() == 2);
        BOOST_TEST(oneOfArray(d, "TParam").size() == 3);
        BOOST_TEST(oneOfArray(d, "TArg").size() == 3);
        BOOST_TEST(oneOfArray(d, "Block").size() == 19);
        BOOST_TEST(oneOfArray(d, "Inline").size() == 16);
    }

    // -- FunctionSymbol properties --------------------------------

    void test_function_symbol_properties()
    {
        dom::Value schema = schema::buildDomJsonSchema();
        dom::Object d = defs(schema);
        dom::Object fp = props(d, "FunctionSymbol");

        // Inherited from Symbol
        BOOST_TEST(fp.exists("name"));
        BOOST_TEST(fp.exists("kind"));
        BOOST_TEST(fp.exists("id"));
        BOOST_TEST(fp.exists("access"));

        // Own members
        BOOST_TEST(fp.exists("returnType"));
        BOOST_TEST(fp.exists("params"));
        BOOST_TEST(fp.exists("isVariadic"));
        BOOST_TEST(fp.exists("isVirtual"));
        BOOST_TEST(fp.exists("isConst"));
        BOOST_TEST(fp.exists("funcClass"));
        BOOST_TEST(fp.exists("noexcept"));
        BOOST_TEST(fp.exists("explicit"));
        BOOST_TEST(fp.exists("template"));

        // params is an array
        BOOST_TEST(fp.get("params").getObject()
                     .get("type").getString() == "array");

        // isVariadic is boolean
        BOOST_TEST(fp.get("isVariadic").getObject()
                     .get("type").getString() == "boolean");

        // returnType is a $ref
        BOOST_TEST(fp.get("returnType").getObject()
                     .exists("$ref"));
    }

    // -- Described enum values ------------------------------------

    void test_described_enum_values()
    {
        dom::Value schema = schema::buildDomJsonSchema();
        dom::Object d = defs(schema);
        dom::Object fp = props(d, "FunctionSymbol");

        // ExtractionMode is a described enum (inherited from Symbol).
        dom::Object extraction = fp.get("extraction").getObject();
        BOOST_TEST(extraction.get("type").getString() == "string");
        dom::Array values = extraction.get("enum").getArray();
        BOOST_TEST(arrayContains(values, "regular"));
        BOOST_TEST(arrayContains(values, "see-below"));
        BOOST_TEST(arrayContains(values, "implementation-defined"));
        BOOST_TEST(arrayContains(values, "dependency"));

        // FunctionClass is a described enum (own member).
        dom::Object funcClass = fp.get("funcClass").getObject();
        BOOST_TEST(funcClass.get("type").getString() == "string");
        dom::Array fcValues = funcClass.get("enum").getArray();
        BOOST_TEST(arrayContains(fcValues, "normal"));
        BOOST_TEST(arrayContains(fcValues, "constructor"));
        BOOST_TEST(arrayContains(fcValues, "destructor"));
    }

    // -- Non-described enums -> string ----------------------------

    void test_non_described_enums()
    {
        dom::Value schema = schema::buildDomJsonSchema();
        dom::Object d = defs(schema);
        dom::Object fp = props(d, "FunctionSymbol");

        // AccessKind serializes as string via manual toString().
        BOOST_TEST(fp.get("access").getObject()
                     .get("type").getString() == "string");

        // ConstexprKind
        BOOST_TEST(fp.get("constexpr").getObject()
                     .get("type").getString() == "string");

        // NoexceptInfo
        BOOST_TEST(fp.get("noexcept").getObject()
                     .get("type").getString() == "string");

        // ExplicitInfo
        BOOST_TEST(fp.get("explicit").getObject()
                     .get("type").getString() == "string");

        // OperatorKind
        BOOST_TEST(fp.get("overloadedOperator").getObject()
                     .get("type").getString() == "string");
    }

    // -- Symbol extension fields ----------------------------------

    void test_symbol_extensions()
    {
        dom::Value schema = schema::buildDomJsonSchema();
        dom::Object d = defs(schema);
        dom::Object fp = props(d, "FunctionSymbol");

        // `class` is the only synthesized field on Symbol's
        // `tag_invoke` overload (see SymbolBase.hpp). Templates
        // use it as a discriminator across Symbol/Type/Name DOM
        // objects. The previous `isRegular`/`isSeeBelow`/etc.
        // convenience booleans have been dropped: templates now
        // derive them from the described `extraction` enum.
        BOOST_TEST(fp.exists("class"));
        BOOST_TEST(fp.get("class").getObject()
                     .get("const").getString() == "symbol");

        dom::Array req = requiredArray(d, "FunctionSymbol");
        BOOST_TEST(arrayContains(req, "class"));
    }

    // -- $meta object ---------------------------------------------

    void test_meta_object()
    {
        dom::Value schema = schema::buildDomJsonSchema();
        dom::Object d = defs(schema);
        dom::Object fp = props(d, "FunctionSymbol");

        BOOST_TEST(fp.exists("$meta"));
        dom::Object meta = fp.get("$meta").getObject();
        BOOST_TEST(meta.get("type").getString() == "object");

        dom::Object metaProps = meta.get("properties").getObject();
        BOOST_TEST(metaProps.exists("type"));
        BOOST_TEST(metaProps.exists("bases"));

        // bases is an array of strings.
        dom::Object bases = metaProps.get("bases").getObject();
        BOOST_TEST(bases.get("type").getString() == "array");
        BOOST_TEST(bases.get("items").getObject()
                       .get("type").getString() == "string");
    }

    // -- Required vs optional -------------------------------------

    void test_required_fields()
    {
        dom::Value schema = schema::buildDomJsonSchema();
        dom::Object d = defs(schema);
        dom::Array req = requiredArray(d, "FunctionSymbol");

        // Booleans are always present (shouldMapValue returns true).
        BOOST_TEST(arrayContains(req, "isVariadic"));
        BOOST_TEST(arrayContains(req, "isVirtual"));
        BOOST_TEST(arrayContains(req, "isConst"));

        // Strings can be empty → not required.
        BOOST_TEST(!arrayContains(req, "name"));

        // ConstexprKind/StorageClassKind can be None → not required.
        BOOST_TEST(!arrayContains(req, "constexpr"));
        BOOST_TEST(!arrayContains(req, "storageClass"));

        // ExprInfo can be empty → not required.
        BOOST_TEST(!arrayContains(req, "requires"));
    }

    // -- Supporting types -----------------------------------------

    void test_supporting_types()
    {
        dom::Value schema = schema::buildDomJsonSchema();
        dom::Object d = defs(schema);

        BOOST_TEST(d.exists("Location"));
        BOOST_TEST(d.exists("Param"));
        BOOST_TEST(d.exists("TemplateInfo"));
        BOOST_TEST(d.exists("SourceInfo"));
        BOOST_TEST(d.exists("BaseInfo"));
        BOOST_TEST(d.exists("FriendInfo"));

        // Location has expected fields.
        dom::Object lp = props(d, "Location");
        BOOST_TEST(lp.exists("shortPath"));
        BOOST_TEST(lp.exists("lineNumber"));
        BOOST_TEST(lp.exists("documented"));
    }

    // -- run ------------------------------------------------------

    void run()
    {
        test_top_level();
        test_all_symbols_in_defs();
        test_polymorphic_variants();
        test_function_symbol_properties();
        test_described_enum_values();
        test_non_described_enums();
        test_symbol_extensions();
        test_meta_object();
        test_required_fields();
        test_supporting_types();
    }
};

} // (anon)

TEST_SUITE(
    DomSchemaWriterTest,
    "clang.mrdocs.Schemas.DomSchemaWriter");

} // mrdocs
