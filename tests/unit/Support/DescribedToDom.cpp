//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/DescribedToDom.hpp>
#include <mrdocs/ADT/Optional.hpp>
#include <test_suite/test_suite.hpp>
#include <string>
#include <vector>

namespace mrdocs {

// ------------------------------------------------------------------
// Fixture types
// ------------------------------------------------------------------

// Fixtures and their descriptor registrations live in a named namespace
// rather than an anonymous one. MRDOCS_DESCRIBE_STRUCT declares ADL
// descriptor functions that are ODR-used only in unevaluated context; in an
// anonymous namespace they would have internal linkage and be "declared but
// never defined", which GCC rejects under -Werror=unused-function. A named
// namespace gives them external linkage, and ADL still finds them next to
// the type.
namespace described_to_dom_test {

// A described struct with one string member: collapses to that string.
struct Word
{
    std::string Text;
};

// A described struct with a mix of members, including an omittable
// string, an optional, and a nested single-text struct.
struct Widget
{
    std::string Name;
    int Count = 0;
    bool Flag = false;
    Optional<std::string> Note;
    Word Label;
    std::vector<std::string> Tags;
};

MRDOCS_DESCRIBE_STRUCT(Word, (), (Text))
MRDOCS_DESCRIBE_STRUCT(Widget, (), (Name, Count, Flag, Note, Label, Tags))

} // described_to_dom_test

namespace {

using described_to_dom_test::Widget;
using described_to_dom_test::Word;

struct DescribedToDomTest
{
    static Widget
    sampleWidget()
    {
        Widget w;
        w.Name = "gadget";
        w.Count = 3;
        w.Flag = true;
        w.Label.Text = "L";
        w.Tags = {"a", "b"};
        return w;
    }

    void
    test_scalar_fields()
    {
        Widget const w = sampleWidget();
        dom::Object const o = describedToDom(w).getObject();
        BOOST_TEST(o.get("name").getString() == "gadget");
        BOOST_TEST(o.get("count").getInteger() == 3);
        BOOST_TEST(o.get("flag").getBool() == true);
    }

    void
    test_meta()
    {
        Widget const w = sampleWidget();
        dom::Object const o = describedToDom(w).getObject();
        dom::Value const meta = o.get("$meta");
        BOOST_TEST(meta.isObject());
        BOOST_TEST(meta.get("type").getString() == "Widget");
        BOOST_TEST(o.exists("$meta"));
    }

    void
    test_single_text_object_collapse()
    {
        // A Word value reflects as its one string, not as an object...
        Word const word{"hello"};
        dom::Value const v = describedToDom(word);
        BOOST_TEST(v.isString());
        BOOST_TEST(v.getString() == "hello");

        // ...and as a member it does too.
        Widget const w = sampleWidget();
        dom::Object const o = describedToDom(w).getObject();
        BOOST_TEST(o.get("label").isString());
        BOOST_TEST(o.get("label").getString() == "L");
    }

    void
    test_omission()
    {
        Widget w = sampleWidget();
        w.Name.clear();   // empty string -> omitted
        w.Note.reset();   // disengaged Optional -> omitted
        dom::Object const o = describedToDom(w).getObject();
        BOOST_TEST(!o.exists("name"));
        BOOST_TEST(o.get("name").isUndefined());
        BOOST_TEST(!o.exists("note"));

        // A present optional is emitted.
        w.Note = "n";
        dom::Object const o2 = describedToDom(w).getObject();
        BOOST_TEST(o2.exists("note"));
        BOOST_TEST(o2.get("note").getString() == "n");
    }

    void
    test_array_field()
    {
        Widget const w = sampleWidget();
        dom::Object const o = describedToDom(w).getObject();
        dom::Value const tags = o.get("tags");
        BOOST_TEST(tags.isArray());
        BOOST_TEST(tags.getArray().size() == 2u);
        BOOST_TEST(tags.getArray().get(0).getString() == "a");
    }

    void
    test_set_writes_back()
    {
        Widget w = sampleWidget();
        dom::Object o = describedToDom(w).getObject();
        o.set("name", "renamed");
        BOOST_TEST(w.Name == "renamed");
    }

    void
    test_set_unknown_field_throws()
    {
        Widget w = sampleWidget();
        dom::Object o = describedToDom(w).getObject();
        bool threw = false;
        try
        {
            o.set("nonexistent", "x");
        }
        catch (std::exception const&)
        {
            threw = true;
        }
        BOOST_TEST(threw);
    }

    void
    test_const_view_is_read_only()
    {
        Widget const w = sampleWidget();
        dom::Object o = describedToDom(w).getObject();
        bool threw = false;
        try
        {
            o.set("name", "x");
        }
        catch (std::exception const&)
        {
            threw = true;
        }
        BOOST_TEST(threw);
    }

    void
    test_array_proxy()
    {
        std::vector<Word> words = {{"one"}, {"two"}};
        dom::Value const v = describedToDom(words);
        BOOST_TEST(v.isArray());
        dom::Array const arr = v.getArray();
        BOOST_TEST(arr.size() == 2u);
        // Word collapses to its single string.
        BOOST_TEST(arr.get(0).getString() == "one");
        BOOST_TEST(arr.get(1).getString() == "two");
    }

    void
    run()
    {
        test_scalar_fields();
        test_meta();
        test_single_text_object_collapse();
        test_omission();
        test_array_field();
        test_set_writes_back();
        test_set_unknown_field_throws();
        test_const_view_is_read_only();
        test_array_proxy();
    }
};

} // namespace

TEST_SUITE(
    DescribedToDomTest,
    "clang.mrdocs.Support.DescribedToDom");

} // namespace mrdocs
