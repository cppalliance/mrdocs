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
#include <mrdocs/Dom/Function.hpp>
#include <test_suite/test_suite.hpp>
#include <stdexcept>


namespace mrdocs {
namespace dom {

struct Function_test
{
    void
    testFunction()
    {
        makeInvocable([](int i) {
            BOOST_TEST(i == 0);
            return 0; })(0);
        makeInvocable([](int i) {
            BOOST_TEST(i == 999);
            return 0; })(999);
        makeInvocable([](std::string_view s) {
            BOOST_TEST(s == "test");
            return 0; })("test");
    }


    void
    function_test()
    {
        // Function()
        {
            Function f;
            BOOST_TEST(f().isUndefined());
        }

        // Function(F const& f)
        {
            Function f([](Array const& args)
            {
                return args.get(0);
            });
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(f(a) == "hello");
        }

        // Function(Function&&)
        {
            Function f1([](Array const& args)
            {
                return args.get(0);
            });
            Function f2(std::move(f1));
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(f2(a) == "hello");
        }

        // Function(Function const&)
        {
            Function f1([](Array const& args)
            {
                return args.get(0);
            });
            Function f2(f1);
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(f1(a) == "hello");
            BOOST_TEST(f2(a) == "hello");
        }

        // operator=(Function&&)
        {
            Function f1([](Array const& args)
            {
                return args.get(0);
            });
            Function f2;
            f2 = std::move(f1);
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(f2(a) == "hello");
        }

        // operator=(Function const&)
        {
            Function f1([](Array const& args)
            {
                return args.get(0);
            });
            Function f2;
            f2 = f1;
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(f1(a) == "hello");
            BOOST_TEST(f2(a) == "hello");
        }

        // impl()
        {
            Function f([](dom::Value const& arg0)
            {
                return arg0;
            });
            BOOST_TEST(f.impl() != nullptr);
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(f.impl()->call(a).value() == "hello");
        }

        // typeKey()
        {
            Function f([](dom::Value const& arg0)
            {
                return arg0;
            });
            BOOST_TEST(std::string_view(f.typeKey()) == "Function");
        }

        // Expected<Value> call(Array const& args)
        {
            Function f([](dom::Value const& arg0)
            {
                return arg0;
            });
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(f.call(a).value() == "hello");
        }

        // operator()(Args&&... args)
        {
            // 0 args
            {
                Function f([]()
                {
                    return "hello";
                });
                BOOST_TEST(f() == "hello");
            }

            // n args
            {
                Function f([](dom::Value const& arg0)
                {
                    return arg0;
                });
                BOOST_TEST(f("hello") == "hello");
            }

            // return void
            {
                bool called = false;
                Function f([&called](dom::Value const&)
                {
                    called = true;
                    return;
                });
                BOOST_TEST(f("hello").isUndefined());
                BOOST_TEST(called);
            }

            // return Expected<void>
            {
                bool called = false;
                auto fn = [&called](dom::Value const&) -> Expected<void>
                {
                    called = true;
                    return Unexpected(Error("error"));
                };
                Function f(fn);
                BOOST_TEST_THROWS(f("hello"), std::runtime_error);
                auto exp = f.call(newArray<DefaultArrayImpl>());
                BOOST_TEST(!exp);
                BOOST_TEST(exp.error().message() == "error");
            }

            // return Value
            {
                Function f([](dom::Value const& arg0)
                {
                    return arg0;
                });
                BOOST_TEST(f("hello") == "hello");
            }

            // missing arguments are replaced with undefined
            {
                Function f([](dom::Value const& arg0)
                {
                    return arg0;
                });
                BOOST_TEST(f().isUndefined());
            }
        }

        // tryInvoke(Args&&... args)
        // same as operator() but returns Expected instead of throwing
        {
            auto fn = [](dom::Value const& arg0) -> Expected<void>
            {
                BOOST_TEST(arg0 == "hello");
                return Unexpected(Error("error"));
            };
            Function f(fn);
            auto exp = f.tryInvoke("hello");
            BOOST_TEST(!exp);
            BOOST_TEST(exp.error().message() == "error");
        }

        // swap(Function& other)
        // swap(Function &lhs, Function& rhs)
        {
            Function f1([]()
            {
                return "hello";
            });
            Function f2([]()
            {
                return "world";
            });
            f1.swap(f2);
            BOOST_TEST(f1() == "world");
            BOOST_TEST(f2() == "hello");
            swap(f1, f2);
            BOOST_TEST(f1() == "hello");
            BOOST_TEST(f2() == "world");
        }

        // makeVariadicInvocable(F&& f)
        {
            auto fn = [](Array const& args)
            {
                BOOST_TEST(args.size() == 2);
                BOOST_TEST(args.get(0) == "hello");
                BOOST_TEST(args.get(1) == "world");
                return args.get(0);
            };
            Function f = makeVariadicInvocable(fn);
            BOOST_TEST(f("hello", "world") == "hello");
        }
    }

    void run()
    {
        testFunction();
        function_test();
    }
};

TEST_SUITE(
    Function_test,
    "clang.mrdocs.dom.Function");

} // dom
} // mrdocs


