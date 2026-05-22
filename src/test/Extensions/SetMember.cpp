//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Gennaro Prota (gennaro.prota@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Extensions/SetMember.hpp>

#include <mrdocs/Dom.hpp>
#include <mrdocs/Metadata/Symbol/Function.hpp>
#include <mrdocs/Metadata/Symbol/Namespace.hpp>
#include <mrdocs/Metadata/Symbol/SymbolID.hpp>
#include <mrdocs/Support/Error.hpp>
#include <mrdocs/Support/Expected.hpp>
#include <test_suite/test_suite.hpp>

#include <cstdio>
#include <string>
#include <string_view>

namespace mrdocs {
namespace {

// ------------------------------------------------------------------
// Fixture
// ------------------------------------------------------------------
//
// `setMemberImpl` only dereferences `state.byId`; it never touches
// `state.corpus`. The tests therefore leave `corpus` as nullptr and
// populate `byId` with hand-built symbols. That keeps the harness
// independent of `CorpusImpl`'s construction machinery (compilation
// database, AST visitor, finalizers, ...) and lets us exercise just
// the error reporting paths of the setter.

bool
contains(std::string const& haystack, std::string_view needle)
{
    return haystack.find(needle) != std::string::npos;
}

// All test cases run against the same per-test fixture: one function
// (returnType is reachable) and one namespace (returnType is missing).
struct Fixture
{
    FunctionSymbol fn{SymbolID("12345678901234567890")};
    NamespaceSymbol ns{SymbolID("abcdefghijklmnopqrst")};
    ExtensionState state{nullptr, {}};

    Fixture()
    {
        state.byId["fn"] = &fn;
        state.byId["ns"] = &ns;
    }
};

// Helper: call `setMemberImpl` and assert that it failed with an
// error message containing `needle`.
void
expectError(
    ExtensionState& state,
    dom::Value const& idArg,
    dom::Value const& fieldArg,
    dom::Value const& valueArg,
    std::string_view needle)
{
    Expected<dom::Value, Error> r =
        setMemberImpl(state, idArg, fieldArg, valueArg);
    BOOST_TEST(!r);
    if (!r)
    {
        bool ok = contains(r.error().message(), needle);
        if (!ok)
        {
            std::fprintf(stderr,
                "SetMemberTest: needle '%.*s' not in: '%s'\n",
                static_cast<int>(needle.size()), needle.data(),
                r.error().message().c_str());
        }
        BOOST_TEST(ok);
    }
}

// ------------------------------------------------------------------
// Tests
// ------------------------------------------------------------------

struct SetMemberTest
{
    // --- Argument-shape errors ---

    void test_id_not_string()
    {
        Fixture f;
        expectError(f.state,
            dom::Value(42),
            dom::Value("name"),
            dom::Value("hi"),
            "argument 1");
    }

    void test_field_not_string()
    {
        Fixture f;
        expectError(f.state,
            dom::Value("fn"),
            dom::Value(42),
            dom::Value("hi"),
            "argument 2");
    }

    // --- Allowlist + lookup errors ---

    void test_unknown_id()
    {
        Fixture f;
        expectError(f.state,
            dom::Value("nope"),
            dom::Value("name"),
            dom::Value("hi"),
            "unknown symbol id");
    }

    void test_off_allowlist_field()
    {
        Fixture f;
        expectError(f.state,
            dom::Value("fn"),
            dom::Value("Bases"),
            dom::Value("hi"),
            "not user-settable");
    }

    void test_allowlisted_but_missing_on_kind()
    {
        // `returnType` is in the allowlist but does not exist on
        // `NamespaceSymbol`.
        Fixture f;
        dom::Object retObj;
        retObj.set("kind", "named");
        expectError(f.state,
            dom::Value("ns"),
            dom::Value("returnType"),
            dom::Value(std::move(retObj)),
            "missing on this symbol");
    }

    // --- Type-mismatch errors ---

    void test_string_field_wants_string()
    {
        // `name` is a string field; passing a boolean must fail.
        Fixture f;
        expectError(f.state,
            dom::Value("fn"),
            dom::Value("name"),
            dom::Value(true),
            "expects a string");
    }

    void test_bool_field_wants_bool()
    {
        // `isCopyFromInherited` is a bool field; passing a string
        // must fail.
        Fixture f;
        expectError(f.state,
            dom::Value("fn"),
            dom::Value("isCopyFromInherited"),
            dom::Value("yes"),
            "expects a boolean");
    }

    void test_enum_wants_enumerator_name()
    {
        // `extraction` is an enum field; passing a boolean must fail
        // before any enumerator lookup.
        Fixture f;
        expectError(f.state,
            dom::Value("fn"),
            dom::Value("extraction"),
            dom::Value(true),
            "expects an enumerator name");
    }

    void test_unknown_enum_name()
    {
        // `extraction` is an enum field; "neon" is not one of its
        // enumerators.
        Fixture f;
        expectError(f.state,
            dom::Value("fn"),
            dom::Value("extraction"),
            dom::Value("neon"),
            "no enumerator named");
    }

    // --- Polymorphic-write errors ---

    void test_polymorphic_not_object()
    {
        // `returnType` is `Polymorphic<TypeBase>`; passing a bare
        // string must fail with "expects an object".
        Fixture f;
        expectError(f.state,
            dom::Value("fn"),
            dom::Value("returnType"),
            dom::Value("named"),
            "polymorphic value");
    }

    void test_polymorphic_missing_kind()
    {
        // `returnType` requires a `kind` field selecting the
        // concrete derived class.
        Fixture f;
        dom::Object retObj;
        expectError(f.state,
            dom::Value("fn"),
            dom::Value("returnType"),
            dom::Value(std::move(retObj)),
            "kind");
    }

    void test_polymorphic_unknown_kind()
    {
        // `returnType` with a kind that is not registered for
        // `TypeBase`.
        Fixture f;
        dom::Object retObj;
        retObj.set("kind", "nonsense-kind");
        expectError(f.state,
            dom::Value("fn"),
            dom::Value("returnType"),
            dom::Value(std::move(retObj)),
            "no derived class with kind");
    }

    void test_polymorphic_unknown_subfield()
    {
        // `returnType` with a valid kind but an unknown sub-field.
        Fixture f;
        dom::Object retObj;
        retObj.set("kind", "named");
        retObj.set("definitely-not-a-real-field", "x");
        expectError(f.state,
            dom::Value("fn"),
            dom::Value("returnType"),
            dom::Value(std::move(retObj)),
            "unknown sub-field");
    }

    // --- Struct-write errors ---

    void test_struct_unknown_subfield()
    {
        // `doc` is `Optional<DocComment>`; passing an object with
        // an unknown key must fail.
        Fixture f;
        dom::Object docObj;
        docObj.set("definitely-not-a-real-field", "x");
        expectError(f.state,
            dom::Value("fn"),
            dom::Value("doc"),
            dom::Value(std::move(docObj)),
            "unknown sub-field");
    }

    // --- Optional null reset (happy path, but adjacent to errors) ---

    void test_optional_null_clears()
    {
        // `doc = null` must succeed (it clears the optional).
        Fixture f;
        Expected<dom::Value, Error> r = setMemberImpl(
            f.state,
            dom::Value("fn"),
            dom::Value("doc"),
            dom::Value(nullptr));
        if (!r)
        {
            std::fprintf(stderr,
                "SetMemberTest::test_optional_null_clears: %s\n",
                r.error().message().c_str());
        }
        BOOST_TEST(r);
    }

    void run()
    {
        // Argument shape
        test_id_not_string();
        test_field_not_string();

        // Allowlist + lookup
        test_unknown_id();
        test_off_allowlist_field();
        test_allowlisted_but_missing_on_kind();

        // Type mismatch
        test_string_field_wants_string();
        test_bool_field_wants_bool();
        test_enum_wants_enumerator_name();
        test_unknown_enum_name();

        // Polymorphic
        test_polymorphic_not_object();
        test_polymorphic_missing_kind();
        test_polymorphic_unknown_kind();
        test_polymorphic_unknown_subfield();

        // Struct
        test_struct_unknown_subfield();

        // Optional null (happy)
        test_optional_null_clears();
    }
};

} // (anon)

TEST_SUITE(
    SetMemberTest,
    "clang.mrdocs.Extensions.SetMember");

} // mrdocs
