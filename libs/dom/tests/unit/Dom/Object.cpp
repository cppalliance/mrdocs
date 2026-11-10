//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Dom.hpp>
#include <test_suite/test_suite.hpp>


namespace mrdocs {
namespace dom {

struct Object_test
{
    void
    object_test()
    {
        // Object()
        {
            Object o;
            BOOST_TEST(o.empty());
        }

        // Object(Object&&)
        {
            Object o1;
            o1.set("hello", "world");
            Object o2(std::move(o1));
            BOOST_TEST(o2.size() == 1);
        }

        // Object(Object const&)
        {
            Object o1;
            o1.set("hello", "world");
            Object o2(o1);
            BOOST_TEST(o2.size() == 1);
            BOOST_TEST(o1.size() == 1);
        }

        // Object(impl_type impl)
        {
            auto impl = std::make_shared<DefaultObjectImpl>();
            impl->set("hello", "world");
            Object o(impl);
            o.set("goodbye", "world");
            BOOST_TEST(o.size() == 2);
        }

        // explicit Object(storage_type list);
        {
            // explicit
            {
                Object::storage_type v;
                v.emplace_back("hello", "world");
                Object o(v);
                o.set("goodbye", "world");
                BOOST_TEST(o.size() == 2);
            }

            // convertible from initializer_list
            {
                Object obj({
                    { "a", 1 },
                    { "b", nullptr },
                    { "c", "test" }});
                BOOST_TEST(obj.size() == 3);
                BOOST_TEST(obj.get("a") == 1);
                BOOST_TEST(obj.get("b").isNull());
                BOOST_TEST(obj.get("c") == "test");
            }
        }

        // Object& operator=(Object&&)
        {
            Object o1;
            o1.set("hello", "world");
            Object o2;
            o2 = std::move(o1);
            BOOST_TEST(o2.size() == 1);
        }

        // Object& operator=(Object const&)
        {
            Object o1;
            o1.set("hello", "world");
            Object o2;
            o2 = o1;
            BOOST_TEST(o2.size() == 1);
            BOOST_TEST(o1.size() == 1);
        }

        // auto impl() const
        {
            {
                Object o;
                o.set("hello", "world");
                BOOST_TEST(o.impl()->size() == 1);
            }

            {
                auto impl = std::make_shared<DefaultObjectImpl>();
                impl->set("hello", "world");
                Object o(impl);
                o.set("goodbye", "world");
                BOOST_TEST(o.impl()->size() == 2);
                BOOST_TEST(impl->size() == 2);
            }
        }

        // char const* typeKey() const noexcept
        {
            Object o;
            BOOST_TEST(std::string_view(o.typeKey()) == "Object");
        }

        // bool empty() const
        {
            Object o;
            BOOST_TEST(o.empty());
            o.set("hello", "world");
            BOOST_TEST(! o.empty());
        }

        // size_type size() const
        {
            Object o;
            BOOST_TEST(o.size() == 0); // NOLINT(*-container-size-empty)
            o.set("hello", "world");
            BOOST_TEST(o.size() == 1);
        }

        // get(std::string_view)
        // at(std::string_view)
        {
            Object o;
            o.set("hello", "world");
            BOOST_TEST(o.get("hello") == "world");
            BOOST_TEST(o.at("hello") == "world");
        }

        // exists(std::string_view)
        {
            Object o;
            o.set("hello", "world");
            BOOST_TEST(o.exists("hello"));
            BOOST_TEST(! o.exists("goodbye"));
        }

        // set(std::string_view, Value)
        {
            Object o;
            o.set("hello", "world");
            BOOST_TEST(o.get("hello") == "world");
        }

        // visit(F&& fn)
        {
            // return void
            {
                Object o;
                o.set("hello", "world1");
                o.set("goodbye", "world2");
                o.visit([](String key, Value value)
                    {
                        BOOST_TEST((key == "hello" || key == "goodbye"));
                        BOOST_TEST((value == "world1" || value == "world2"));
                    });
            }

            // return bool
            {
                Object o;
                o.set("hello", "world");
                o.set("goodbye", "world");
                int count = 0;
                bool exp = o.visit([&count](String key, Value value)
                    {
                        BOOST_TEST(key == "hello");
                        BOOST_TEST(value == "world");
                        ++count;
                        return false;
                    });
                BOOST_TEST(!exp);
                BOOST_TEST(count == 1);
            }

            // return Expected<void>
            {
                Object o;
                o.set("hello", "world");
                o.set("goodbye", "world");
                int count = 0;
                auto exp = o.visit([&count](String key, Value value) -> Expected<void>
                    {
                        BOOST_TEST(key == "hello");
                        BOOST_TEST(value == "world");
                        ++count;
                        return Unexpected(Error("error"));
                    });
                BOOST_TEST(!exp);
                BOOST_TEST(exp.error().message() == "error");
                BOOST_TEST(count == 1);
            }
        }

        // void swap(Object&)
        // friend void swap(Object&, Object&)
        {
            Object o1;
            o1.set("hello", "world");
            Object o2;
            o2.set("goodbye", "world");
            o1.swap(o2);
            BOOST_TEST(o1.size() == 1);
            BOOST_TEST(o2.size() == 1);
            BOOST_TEST(o1.get("goodbye") == "world");
            BOOST_TEST(o2.get("hello") == "world");
            swap(o1, o2);
            BOOST_TEST(o1.size() == 1);
            BOOST_TEST(o2.size() == 1);
            BOOST_TEST(o1.get("hello") == "world");
            BOOST_TEST(o2.get("goodbye") == "world");
        }

        // operator==(Object const&, Object const&)
        // operator!=(Object const&, Object const&)
        {
            Object o1;
            o1.set("hello", "world");
            Object o2 = o1;
            BOOST_TEST(o1 == o2);
            BOOST_TEST(o2 == o1);
            Object o3;
            o3.set("hello", "world");
            BOOST_TEST(o1 != o3);
            BOOST_TEST(o3 != o2);
        }

        // toString(Object const&)
        {
            // Behave same as JS:
            // x = {hello: 'world'}
            // x.toString() == '[object Object]'
            Object o;
            o.set("hello", "world");
            BOOST_TEST(toString(o) == "[object Object]");
        }
    }

    void run()
    {
        object_test();
    }
};

TEST_SUITE(
    Object_test,
    "clang.mrdocs.dom.Object");

} // dom
} // mrdocs
