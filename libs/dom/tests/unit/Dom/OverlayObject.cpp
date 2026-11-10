//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom/OverlayObject.hpp>
#include <test_suite/test_suite.hpp>
#include <string>
#include <vector>

namespace mrdocs {
namespace dom {

struct OverlayObject_test
{
    static Object
    makeParent()
    {
        Object parent;
        parent.set("a", 1);
        parent.set("b", 2);
        return parent;
    }

    static Object
    makeChild()
    {
        Object child;
        child.set("b", 20);
        child.set("c", 30);
        return child;
    }

    static std::vector<std::string>
    keysOf(Object const& object)
    {
        std::vector<std::string> keys;
        object.visit([&](String key, Value const&)
            {
                keys.push_back(std::string(key.get()));
                return true;
            });
        return keys;
    }

    void
    testGet()
    {
        Object const frame = overlayObject(makeChild(), makeParent());
        // A key of the child shadows the parent's
        BOOST_TEST(frame.get("b") == 20);
        // A key only the child has, and one only the parent has
        BOOST_TEST(frame.get("c") == 30);
        BOOST_TEST(frame.get("a") == 1);
        BOOST_TEST(frame.get("missing").isUndefined());
    }

    void
    testExists()
    {
        Object const frame = overlayObject(makeChild(), makeParent());
        BOOST_TEST(frame.exists("a"));
        BOOST_TEST(frame.exists("b"));
        BOOST_TEST(frame.exists("c"));
        BOOST_TEST_NOT(frame.exists("missing"));
    }

    // A key both layers have counts once
    void
    testSize()
    {
        Object const frame = overlayObject(makeChild(), makeParent());
        BOOST_TEST(frame.size() == 3u);
        BOOST_TEST(overlayObject(makeParent()).size() == 2u);
    }

    // The keys of the child come first, and a shadowed key is listed once
    void
    testVisit()
    {
        Object const frame = overlayObject(makeChild(), makeParent());
        std::vector<std::string> const keys = keysOf(frame);
        BOOST_TEST(keys.size() == 3u);
        if (keys.size() == 3u)
        {
            BOOST_TEST(keys[0] == "b");
            BOOST_TEST(keys[1] == "c");
            BOOST_TEST(keys[2] == "a");
        }

        // The visitor stops the walk
        int calls = 0;
        bool const finished = frame.visit([&](String, Value const&)
            {
                ++calls;
                return false;
            });
        BOOST_TEST_NOT(finished);
        BOOST_TEST(calls == 1);
    }

    // A write goes to the child, so the parent keeps its keys
    void
    testSet()
    {
        Object parent = makeParent();
        Object frame = overlayObject(parent);
        frame.set("a", 100);
        frame.set("z", 26);
        BOOST_TEST(frame.get("a") == 100);
        BOOST_TEST(frame.get("z") == 26);
        BOOST_TEST(parent.get("a") == 1);
        BOOST_TEST_NOT(parent.exists("z"));
    }

    // An overlay of overlays looks through every layer, and the nearest
    // layer wins
    void
    testNested()
    {
        Object top;
        top.set("a", 1000);
        Object const middle = overlayObject(makeChild(), makeParent());
        Object const frame = overlayObject(top, middle);
        BOOST_TEST(frame.get("a") == 1000);
        BOOST_TEST(frame.get("b") == 20);
        BOOST_TEST(frame.get("c") == 30);
        BOOST_TEST(frame.size() == 3u);
        std::vector<std::string> const keys = keysOf(frame);
        BOOST_TEST(keys.size() == 3u);

        Object const deeper = overlayObject(makeChild(), overlayObject(makeParent()));
        BOOST_TEST(deeper.get("a") == 1);
        BOOST_TEST(deeper.get("b") == 20);
        BOOST_TEST(deeper.size() == 3u);
    }

    // The layers of a child that is itself an overlay keep their order,
    // ahead of the layers of the parent
    void
    testChildOverlayOrder()
    {
        Object q0;
        q0.set("k", "q0");
        Object z;
        z.set("z", 1);
        Object const q = overlayObject(z, q0);
        Object const c = overlayObject(Object(), q);
        Object p;
        p.set("k", "P");
        Object const f = overlayObject(c, p);
        BOOST_TEST(f.get("k") == "q0");
        BOOST_TEST(f.get("z") == 1);
        BOOST_TEST(f.size() == 2u);
        BOOST_TEST(keysOf(f).size() == 2u);
    }

    void
    run()
    {
        testGet();
        testExists();
        testSize();
        testVisit();
        testSet();
        testNested();
        testChildOverlayOrder();
    }
};

TEST_SUITE(
    OverlayObject_test,
    "clang.mrdocs.dom.OverlayObject");

} // dom
} // mrdocs
