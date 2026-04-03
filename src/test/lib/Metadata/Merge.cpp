//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Symbol/Enum.hpp>
#include <mrdocs/Metadata/Symbol/Friend.hpp>
#include <mrdocs/Metadata/Symbol/Function.hpp>
#include <mrdocs/Metadata/Symbol/Namespace.hpp>
#include <mrdocs/Metadata/Symbol/Param.hpp>
#include <mrdocs/Metadata/Symbol/Record.hpp>
#include <mrdocs/Metadata/Name/IdentifierName.hpp>
#include <mrdocs/Metadata/Type/NamedType.hpp>
#include <mrdocs/Support/MergeReflectedType.hpp>
#include <test_suite/test_suite.hpp>

namespace mrdocs {
namespace {

// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------

SymbolID const id1("12345678901234567890");
SymbolID const id2("abcdefghijklmnopqrst");

Polymorphic<Type>
makeNamedType(std::string identifier)
{
    NamedType nt;
    nt.Name->Identifier = std::move(identifier);
    return Polymorphic<Type>(std::move(nt));
}

FunctionSymbol
makeFunc()
{
    FunctionSymbol f(id1);
    f.Name = "f";
    return f;
}

EnumSymbol
makeEnum()
{
    EnumSymbol e(id1);
    e.Name = "E";
    return e;
}

// ------------------------------------------------------------------

struct MergeTest
{
    // -- Param ----------------------------------------------------

    void test_param_type_placeholder_takes_src()
    {
        Param dst;  // Type = AutoType{} (placeholder)
        Param src;
        src.Type = makeNamedType("int");

        merge(dst, std::move(src));
        BOOST_TEST(dst.Type->isNamed());
    }

    void test_param_type_non_placeholder_keeps_dst()
    {
        Param dst;
        dst.Type = makeNamedType("int");
        Param src;
        src.Type = makeNamedType("double");

        merge(dst, std::move(src));
        BOOST_TEST(dst.Type->asNamed().Name->Identifier == "int");
    }

    void test_param_name_empty_takes_src()
    {
        Param dst;
        Param src;
        src.Name = "x";

        merge(dst, std::move(src));
        BOOST_TEST(dst.Name.has_value());
        BOOST_TEST(*dst.Name == "x");
    }

    void test_param_name_set_keeps_dst()
    {
        Param dst;
        dst.Name = "a";
        Param src;
        src.Name = "b";

        merge(dst, std::move(src));
        BOOST_TEST(*dst.Name == "a");
    }

    void test_param_default_empty_takes_src()
    {
        Param dst;
        Param src;
        src.Default = "42";

        merge(dst, std::move(src));
        BOOST_TEST(dst.Default.has_value());
        BOOST_TEST(*dst.Default == "42");
    }

    // -- vector<Param> --------------------------------------------

    void test_param_vector_element_wise()
    {
        Param p1;
        p1.Name = "x";
        std::vector<Param> dst = { std::move(p1) };

        Param s1;
        s1.Default = "0";
        std::vector<Param> src = { std::move(s1) };

        merge(dst, std::move(src));
        BOOST_TEST(dst.size() == 1u);
        BOOST_TEST(*dst[0].Name == "x");
        BOOST_TEST(*dst[0].Default == "0");
    }

    void test_param_vector_src_has_extras()
    {
        std::vector<Param> dst;
        Param s1;
        s1.Name = "a";
        Param s2;
        s2.Name = "b";
        std::vector<Param> src = { std::move(s1), std::move(s2) };

        merge(dst, std::move(src));
        BOOST_TEST(dst.size() == 2u);
        BOOST_TEST(*dst[0].Name == "a");
        BOOST_TEST(*dst[1].Name == "b");
    }

    void test_param_vector_dst_has_extras()
    {
        Param p1;
        p1.Name = "x";
        Param p2;
        p2.Name = "y";
        std::vector<Param> dst = { std::move(p1), std::move(p2) };
        std::vector<Param> src;

        merge(dst, std::move(src));
        BOOST_TEST(dst.size() == 2u);
    }

    // -- FriendInfo -----------------------------------------------

    void test_friend_id_invalid_takes_src()
    {
        FriendInfo dst;
        FriendInfo src;
        src.id = id1;

        merge(dst, std::move(src));
        BOOST_TEST(dst.id == id1);
    }

    void test_friend_id_valid_keeps_dst()
    {
        FriendInfo dst;
        dst.id = id1;
        FriendInfo src;
        src.id = id2;

        merge(dst, std::move(src));
        BOOST_TEST(dst.id == id1);
    }

    void test_friend_type_empty_takes_src()
    {
        FriendInfo dst;
        FriendInfo src;
        src.Type = Polymorphic<Type>(NamedType{});

        merge(dst, std::move(src));
        BOOST_TEST(dst.Type.has_value());
    }

    // -- vector<FriendInfo> ---------------------------------------

    void test_friend_vector_dedup_by_id()
    {
        FriendInfo f1;
        f1.id = id1;
        std::vector<FriendInfo> dst = { f1 };

        FriendInfo s1;
        s1.id = id1;  // duplicate
        FriendInfo s2;
        s2.id = id2;  // new
        std::vector<FriendInfo> src = { std::move(s1), std::move(s2) };

        merge(dst, std::move(src));
        BOOST_TEST(dst.size() == 2u);
    }

    void test_friend_vector_empty_dst()
    {
        std::vector<FriendInfo> dst;
        FriendInfo s1;
        s1.id = id1;
        std::vector<FriendInfo> src = { std::move(s1) };

        merge(dst, std::move(src));
        BOOST_TEST(dst.size() == 1u);
        BOOST_TEST(dst[0].id == id1);
    }

    // -- Symbol (base class fields) -------------------------------

    void test_symbol_name_empty_takes_src()
    {
        auto dst = makeFunc();
        dst.Name = "";
        auto src = makeFunc();
        src.Name = "foo";

        merge(static_cast<Symbol&>(dst), static_cast<Symbol&&>(src));
        BOOST_TEST(dst.Name == "foo");
    }

    void test_symbol_name_set_keeps_dst()
    {
        auto dst = makeFunc();
        dst.Name = "bar";
        auto src = makeFunc();
        src.Name = "baz";

        merge(static_cast<Symbol&>(dst), static_cast<Symbol&&>(src));
        BOOST_TEST(dst.Name == "bar");
    }

    void test_symbol_parent_invalid_takes_src()
    {
        auto dst = makeFunc();
        auto src = makeFunc();
        src.Parent = id2;

        merge(static_cast<Symbol&>(dst), static_cast<Symbol&&>(src));
        BOOST_TEST(dst.Parent == id2);
    }

    void test_symbol_parent_valid_keeps_dst()
    {
        auto dst = makeFunc();
        dst.Parent = id1;
        auto src = makeFunc();
        src.Parent = id2;

        merge(static_cast<Symbol&>(dst), static_cast<Symbol&&>(src));
        BOOST_TEST(dst.Parent == id1);
    }

    void test_symbol_access_none_takes_src()
    {
        auto dst = makeFunc();
        auto src = makeFunc();
        src.Access = AccessKind::Public;

        merge(static_cast<Symbol&>(dst), static_cast<Symbol&&>(src));
        BOOST_TEST(dst.Access == AccessKind::Public);
    }

    void test_symbol_bool_or_merge()
    {
        auto dst = makeFunc();
        auto src = makeFunc();
        src.IsCopyFromInherited = true;

        merge(static_cast<Symbol&>(dst), static_cast<Symbol&&>(src));
        BOOST_TEST(dst.IsCopyFromInherited);
    }

    // -- FunctionSymbol -------------------------------------------

    void test_func_noexcept_implicit_takes_src()
    {
        auto dst = makeFunc();
        auto src = makeFunc();
        src.Noexcept.Implicit = false;
        src.Noexcept.Kind = NoexceptKind::True;

        merge(dst, std::move(src));
        BOOST_TEST(!dst.Noexcept.Implicit);
        BOOST_TEST(dst.Noexcept.Kind == NoexceptKind::True);
    }

    void test_func_noexcept_explicit_keeps_dst()
    {
        auto dst = makeFunc();
        dst.Noexcept.Implicit = false;
        dst.Noexcept.Kind = NoexceptKind::True;
        auto src = makeFunc();
        src.Noexcept.Implicit = false;
        src.Noexcept.Kind = NoexceptKind::False;

        merge(dst, std::move(src));
        BOOST_TEST(dst.Noexcept.Kind == NoexceptKind::True);
    }

    void test_func_explicit_implicit_takes_src()
    {
        auto dst = makeFunc();
        auto src = makeFunc();
        src.Explicit.Implicit = false;
        src.Explicit.Kind = ExplicitKind::True;

        merge(dst, std::move(src));
        BOOST_TEST(!dst.Explicit.Implicit);
    }

    void test_func_attributes_dedup()
    {
        auto dst = makeFunc();
        dst.Attributes.push_back("nodiscard");
        auto src = makeFunc();
        src.Attributes.push_back("nodiscard");
        src.Attributes.push_back("deprecated");

        merge(dst, std::move(src));
        BOOST_TEST(dst.Attributes.size() == 2u);
    }

    void test_func_bool_fields_or()
    {
        auto dst = makeFunc();
        auto src = makeFunc();
        src.IsRecordMethod = true;
        src.IsVariadic = true;

        merge(dst, std::move(src));
        BOOST_TEST(dst.IsRecordMethod);
        BOOST_TEST(dst.IsVariadic);
    }

    void test_func_enum_takes_nonzero()
    {
        auto dst = makeFunc();
        auto src = makeFunc();
        src.FuncClass = FunctionClass::Constructor;

        merge(dst, std::move(src));
        BOOST_TEST(dst.FuncClass == FunctionClass::Constructor);
    }

    // -- EnumSymbol -----------------------------------------------

    void test_enum_constants_dedup()
    {
        auto dst = makeEnum();
        dst.Constants.push_back(id1);

        auto src = makeEnum();
        src.Constants.push_back(id1);  // duplicate
        src.Constants.push_back(id2);  // new

        merge(dst, std::move(src));
        BOOST_TEST(dst.Constants.size() == 2u);
    }

    void test_enum_scoped_bool_or()
    {
        auto dst = makeEnum();
        auto src = makeEnum();
        src.Scoped = true;

        merge(dst, std::move(src));
        BOOST_TEST(dst.Scoped);
    }

    // -- RecordTranche --------------------------------------------

    void test_record_tranche_dedup_append()
    {
        RecordTranche dst;
        dst.Records.push_back(id1);

        RecordTranche src;
        src.Records.push_back(id1);  // duplicate
        src.Records.push_back(id2);  // new
        src.Functions.push_back(id1);

        merge(dst, std::move(src));
        BOOST_TEST(dst.Records.size() == 2u);
        BOOST_TEST(dst.Functions.size() == 1u);
    }

    // -- NamespaceTranche -----------------------------------------

    void test_namespace_tranche_dedup_append()
    {
        NamespaceTranche dst;
        dst.Namespaces.push_back(id1);
        dst.Usings.push_back(id2);

        NamespaceTranche src;
        src.Namespaces.push_back(id1);  // duplicate
        src.Namespaces.push_back(id2);  // new
        src.Usings.push_back(id2);      // duplicate

        merge(dst, std::move(src));
        BOOST_TEST(dst.Namespaces.size() == 2u);
        BOOST_TEST(dst.Usings.size() == 1u);
    }

    // -- Name -----------------------------------------------------

    void test_name_identifier_empty_takes_src()
    {
        IdentifierName dst;
        IdentifierName src;
        src.Identifier = "ns";
        src.id = id1;

        merge(static_cast<Name&>(dst), static_cast<Name&&>(src));
        BOOST_TEST(dst.Identifier == "ns");
        BOOST_TEST(dst.id == id1);
    }

    void test_name_identifier_set_keeps_dst()
    {
        IdentifierName dst;
        dst.Identifier = "original";
        IdentifierName src;
        src.Identifier = "other";

        merge(static_cast<Name&>(dst), static_cast<Name&&>(src));
        BOOST_TEST(dst.Identifier == "original");
    }

    // -- run ------------------------------------------------------

    void run()
    {
        test_param_type_placeholder_takes_src();
        test_param_type_non_placeholder_keeps_dst();
        test_param_name_empty_takes_src();
        test_param_name_set_keeps_dst();
        test_param_default_empty_takes_src();

        test_param_vector_element_wise();
        test_param_vector_src_has_extras();
        test_param_vector_dst_has_extras();

        test_friend_id_invalid_takes_src();
        test_friend_id_valid_keeps_dst();
        test_friend_type_empty_takes_src();

        test_friend_vector_dedup_by_id();
        test_friend_vector_empty_dst();

        test_symbol_name_empty_takes_src();
        test_symbol_name_set_keeps_dst();
        test_symbol_parent_invalid_takes_src();
        test_symbol_parent_valid_keeps_dst();
        test_symbol_access_none_takes_src();
        test_symbol_bool_or_merge();

        test_func_noexcept_implicit_takes_src();
        test_func_noexcept_explicit_keeps_dst();
        test_func_explicit_implicit_takes_src();
        test_func_attributes_dedup();
        test_func_bool_fields_or();
        test_func_enum_takes_nonzero();

        test_enum_constants_dedup();
        test_enum_scoped_bool_or();

        test_record_tranche_dedup_append();
        test_namespace_tranche_dedup_append();

        test_name_identifier_empty_takes_src();
        test_name_identifier_set_keeps_dst();
    }
};

} // (anon)

TEST_SUITE(MergeTest, "clang.mrdocs.Metadata.Merge");

} // mrdocs
