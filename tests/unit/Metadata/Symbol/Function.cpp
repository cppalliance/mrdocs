//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Metadata/Symbol/Function.hpp>
#include <mrdocs/Metadata/Type/LValueReferenceType.hpp>
#include <mrdocs/Metadata/Type/NamedType.hpp>
#include <mrdocs/Metadata/Type/RValueReferenceType.hpp>
#include <test_suite/test_suite.hpp>

namespace mrdocs {
namespace {

// A fixed parent ID for the record that owns the member functions.
SymbolID const recordId("12345678901234567890");

// ------------------------------------------------------------------
// Helpers to build Param objects with specific types
// ------------------------------------------------------------------

/// Make a NamedType whose namedSymbol() returns @p id.
Polymorphic<Type>
makeNamedType(SymbolID id)
{
    NamedType nt;
    nt.Name->id = id;
    return Polymorphic<Type>(std::move(nt));
}

/// Make an lvalue reference to a NamedType with the given symbol ID.
Polymorphic<Type>
makeLValueRef(SymbolID id)
{
    LValueReferenceType lref;
    lref.PointeeType = makeNamedType(id);
    return Polymorphic<Type>(std::move(lref));
}

/// Make an rvalue reference to a NamedType with the given symbol ID.
Polymorphic<Type>
makeRValueRef(SymbolID id)
{
    RValueReferenceType rref;
    rref.PointeeType = makeNamedType(id);
    return Polymorphic<Type>(std::move(rref));
}

/// Make a parameter with a given type and no default.
Param
makeParam(Polymorphic<Type> type)
{
    Param p;
    p.Type = std::move(type);
    return p;
}

/// Make a parameter with a given type and a default value.
Param
makeParamWithDefault(Polymorphic<Type> type)
{
    Param p;
    p.Type = std::move(type);
    p.Default.emplace("0");
    return p;
}

/// Make a parameter whose type is a pack expansion.
Param
makePackParam()
{
    Param p;
    p.Type->IsPackExpansion = true;
    return p;
}

// ------------------------------------------------------------------
// Helper to build a FunctionSymbol with specific fields
// ------------------------------------------------------------------

FunctionSymbol
makeFunc()
{
    FunctionSymbol f(SymbolID{});
    f.Parent = recordId;
    return f;
}

// ==================================================================
// Tests
// ==================================================================

struct FunctionSymbolTest
{
    // ------ isDefaultConstructor ------

    void test_default_ctor_no_params()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        BOOST_TEST(isDefaultConstructor(f));
    }

    void test_default_ctor_all_defaults()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParamWithDefault(makeNamedType(SymbolID{})));
        f.Params.push_back(makeParamWithDefault(makeNamedType(SymbolID{})));
        BOOST_TEST(isDefaultConstructor(f));
    }

    void test_default_ctor_pack_expansion()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makePackParam());
        BOOST_TEST(isDefaultConstructor(f));
    }

    void test_default_ctor_mixed_defaults_and_packs()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParamWithDefault(makeNamedType(SymbolID{})));
        f.Params.push_back(makePackParam());
        BOOST_TEST(isDefaultConstructor(f));
    }

    void test_default_ctor_param_without_default()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeNamedType(SymbolID{})));
        BOOST_TEST(!isDefaultConstructor(f));
    }

    void test_default_ctor_not_a_constructor()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Normal;
        BOOST_TEST(!isDefaultConstructor(f));
    }

    // ------ isCopyConstructor ------

    void test_copy_ctor_single_lvalue_ref()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        BOOST_TEST(isCopyConstructor(f));
    }

    void test_copy_ctor_lvalue_ref_plus_defaults()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        f.Params.push_back(makeParamWithDefault(makeNamedType(SymbolID{})));
        BOOST_TEST(isCopyConstructor(f));
    }

    void test_copy_ctor_lvalue_ref_plus_non_default()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        f.Params.push_back(makeParam(makeNamedType(SymbolID{})));
        BOOST_TEST(!isCopyConstructor(f));
    }

    void test_copy_ctor_wrong_record()
    {
        SymbolID otherId("abcdefghijklmnopqrst");
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeLValueRef(otherId)));
        BOOST_TEST(!isCopyConstructor(f));
    }

    void test_copy_ctor_rvalue_ref()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeRValueRef(recordId)));
        BOOST_TEST(!isCopyConstructor(f));
    }

    void test_copy_ctor_template()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Template.emplace();
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        BOOST_TEST(!isCopyConstructor(f));
    }

    void test_copy_ctor_no_params()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        BOOST_TEST(!isCopyConstructor(f));
    }

    void test_copy_ctor_not_a_constructor()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Normal;
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        BOOST_TEST(!isCopyConstructor(f));
    }

    // ------ isMoveConstructor ------

    void test_move_ctor_single_rvalue_ref()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeRValueRef(recordId)));
        BOOST_TEST(isMoveConstructor(f));
    }

    void test_move_ctor_rvalue_ref_plus_defaults()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeRValueRef(recordId)));
        f.Params.push_back(makeParamWithDefault(makeNamedType(SymbolID{})));
        BOOST_TEST(isMoveConstructor(f));
    }

    void test_move_ctor_rvalue_ref_plus_non_default()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeRValueRef(recordId)));
        f.Params.push_back(makeParam(makeNamedType(SymbolID{})));
        BOOST_TEST(!isMoveConstructor(f));
    }

    void test_move_ctor_lvalue_ref()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        BOOST_TEST(!isMoveConstructor(f));
    }

    void test_move_ctor_template()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Template.emplace();
        f.Params.push_back(makeParam(makeRValueRef(recordId)));
        BOOST_TEST(!isMoveConstructor(f));
    }

    // ------ isCopyAssignment ------

    void test_copy_assign_lvalue_ref()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        BOOST_TEST(isCopyAssignment(f));
    }

    void test_copy_assign_by_value()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Params.push_back(makeParam(makeNamedType(recordId)));
        BOOST_TEST(isCopyAssignment(f));
    }

    void test_copy_assign_rvalue_ref()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Params.push_back(makeParam(makeRValueRef(recordId)));
        BOOST_TEST(!isCopyAssignment(f));
    }

    void test_copy_assign_wrong_operator()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::PlusEqual;
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        BOOST_TEST(!isCopyAssignment(f));
    }

    void test_copy_assign_template()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Template.emplace();
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        BOOST_TEST(!isCopyAssignment(f));
    }

    void test_copy_assign_wrong_record()
    {
        SymbolID otherId("abcdefghijklmnopqrst");
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Params.push_back(makeParam(makeLValueRef(otherId)));
        BOOST_TEST(!isCopyAssignment(f));
    }

    void test_copy_assign_wrong_record_by_value()
    {
        SymbolID otherId("abcdefghijklmnopqrst");
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Params.push_back(makeParam(makeNamedType(otherId)));
        BOOST_TEST(!isCopyAssignment(f));
    }

    // ------ isMoveAssignment ------

    void test_move_assign_rvalue_ref()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Params.push_back(makeParam(makeRValueRef(recordId)));
        BOOST_TEST(isMoveAssignment(f));
    }

    void test_move_assign_lvalue_ref()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        BOOST_TEST(!isMoveAssignment(f));
    }

    void test_move_assign_by_value()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Params.push_back(makeParam(makeNamedType(recordId)));
        BOOST_TEST(!isMoveAssignment(f));
    }

    void test_move_assign_template()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Template.emplace();
        f.Params.push_back(makeParam(makeRValueRef(recordId)));
        BOOST_TEST(!isMoveAssignment(f));
    }

    void test_move_assign_wrong_operator()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::PlusEqual;
        f.Params.push_back(makeParam(makeRValueRef(recordId)));
        BOOST_TEST(!isMoveAssignment(f));
    }

    // ------ isSpecialMemberFunction ------

    void test_special_destructor()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Destructor;
        BOOST_TEST(isSpecialMemberFunction(f));
    }

    void test_special_default_constructor()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        BOOST_TEST(isSpecialMemberFunction(f));
    }

    void test_special_copy_constructor()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        BOOST_TEST(isSpecialMemberFunction(f));
    }

    void test_special_move_constructor()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Constructor;
        f.Params.push_back(makeParam(makeRValueRef(recordId)));
        BOOST_TEST(isSpecialMemberFunction(f));
    }

    void test_special_copy_assignment()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Params.push_back(makeParam(makeLValueRef(recordId)));
        BOOST_TEST(isSpecialMemberFunction(f));
    }

    void test_special_move_assignment()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Equal;
        f.Params.push_back(makeParam(makeRValueRef(recordId)));
        BOOST_TEST(isSpecialMemberFunction(f));
    }

    void test_special_normal_function()
    {
        FunctionSymbol f = makeFunc();
        f.FuncClass = FunctionClass::Normal;
        BOOST_TEST(!isSpecialMemberFunction(f));
    }

    void test_special_call_operator()
    {
        FunctionSymbol f = makeFunc();
        f.OverloadedOperator = OperatorKind::Call;
        f.Params.push_back(makeParam(makeNamedType(SymbolID{})));
        BOOST_TEST(!isSpecialMemberFunction(f));
    }

    // ------ run ------

    void run()
    {
        // isDefaultConstructor
        test_default_ctor_no_params();
        test_default_ctor_all_defaults();
        test_default_ctor_pack_expansion();
        test_default_ctor_mixed_defaults_and_packs();
        test_default_ctor_param_without_default();
        test_default_ctor_not_a_constructor();

        // isCopyConstructor
        test_copy_ctor_single_lvalue_ref();
        test_copy_ctor_lvalue_ref_plus_defaults();
        test_copy_ctor_lvalue_ref_plus_non_default();
        test_copy_ctor_wrong_record();
        test_copy_ctor_rvalue_ref();
        test_copy_ctor_template();
        test_copy_ctor_no_params();
        test_copy_ctor_not_a_constructor();

        // isMoveConstructor
        test_move_ctor_single_rvalue_ref();
        test_move_ctor_rvalue_ref_plus_defaults();
        test_move_ctor_rvalue_ref_plus_non_default();
        test_move_ctor_lvalue_ref();
        test_move_ctor_template();

        // isCopyAssignment
        test_copy_assign_lvalue_ref();
        test_copy_assign_by_value();
        test_copy_assign_rvalue_ref();
        test_copy_assign_wrong_operator();
        test_copy_assign_template();
        test_copy_assign_wrong_record();
        test_copy_assign_wrong_record_by_value();

        // isMoveAssignment
        test_move_assign_rvalue_ref();
        test_move_assign_lvalue_ref();
        test_move_assign_by_value();
        test_move_assign_template();
        test_move_assign_wrong_operator();

        // isSpecialMemberFunction
        test_special_destructor();
        test_special_default_constructor();
        test_special_copy_constructor();
        test_special_move_constructor();
        test_special_copy_assignment();
        test_special_move_assignment();
        test_special_normal_function();
        test_special_call_operator();
    }
};

} // (anon)

TEST_SUITE(
    FunctionSymbolTest,
    "clang.mrdocs.Metadata.Symbol.Function");

} // mrdocs
