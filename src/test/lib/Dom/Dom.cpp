//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdox
//

#include <mrdox/Dom.hpp>
#include <test_suite/test_suite.hpp>

namespace clang {
namespace mrdox {
namespace dom {

struct Dom_test
{
    void
    kind_test()
    {
        BOOST_TEST(toString(Kind::Undefined) == "undefined");
        BOOST_TEST(toString(Kind::Null) == "null");
        BOOST_TEST(toString(Kind::Boolean) == "boolean");
        BOOST_TEST(toString(Kind::Integer) == "integer");
        BOOST_TEST(toString(Kind::String) == "string");
        BOOST_TEST(toString(Kind::SafeString) == "safeString");
        BOOST_TEST(toString(Kind::Array) == "array");
        BOOST_TEST(toString(Kind::Object) == "object");
        BOOST_TEST(toString(Kind::Function) == "function");
        BOOST_TEST(toString(static_cast<Kind>(123)) == "unknown");
    }

    void
    string_test()
    {
        // String()
        {
            String s;
            BOOST_TEST(s.empty());
        }

        // String(String&&)
        {
            String s1("hello");
            String s2(std::move(s1));
            BOOST_TEST(s2 == "hello");
        }

        // String(String const&)
        {
            String s1("hello");
            String s2(s1);
            BOOST_TEST(s2 == "hello");
            BOOST_TEST(s1 == "hello");
        }

        // String(std::string_view s)
        {
            String s(std::string_view("hello"));
            BOOST_TEST(s == "hello");
        }

        // String(StringLike const& s)
        {
            String s(std::string("hello"));
            BOOST_TEST(s == "hello");
        }

        // String(char const(&psz)[N])
        {
            String s("hello");
            BOOST_TEST(s == "hello");
        }

        // operator=(String&&)
        {
            String s1("hello");
            String s2;
            s2 = std::move(s1);
            BOOST_TEST(s2 == "hello");
        }

        // operator=(String const&)
        {
            String s1("hello");
            String s2;
            s2 = s1;
            BOOST_TEST(s2 == "hello");
            BOOST_TEST(s1 == "hello");
        }

        // empty()
        {
            String s;
            BOOST_TEST(s.empty());
            s = "hello";
            BOOST_TEST(! s.empty());
        }

        // std::string_view get() const
        {
            String s("hello");
            BOOST_TEST(s.get() == "hello");
        }

        // operator std::string_view()
        {
            String s("hello");
            std::string_view sv(s);
            BOOST_TEST(sv == "hello");
        }

        // std::string str()
        {
            String s("hello");
            BOOST_TEST(s.str() == "hello");
        }

        // std::size_t size() const
        {
            String s("hello");
            BOOST_TEST(s.size() == 5);
        }

        // char const* data() const
        {
            String s("hello");
            BOOST_TEST(s.data() == s.get().data());
        }

        // char const* c_str() const
        {
            String s("hello");
            BOOST_TEST(s.c_str() == s.get().data());
        }

        // swap(String&)
        {
            String s1("hello");
            String s2("world");
            s1.swap(s2);
            BOOST_TEST(s1 == "world");
            BOOST_TEST(s2 == "hello");
        }

        // swap(String&, String&)
        {
            String s1("hello");
            String s2("world");
            swap(s1, s2);
            BOOST_TEST(s1 == "world");
            BOOST_TEST(s2 == "hello");
        }

        // operator==(String const& lhs, StringLike const& rhs)
        {
            String s1("hello");
            std::string s2("hello");
            BOOST_TEST(s1 == s2);
            BOOST_TEST(s2 == s1);
        }

        // operator!=(String const& lhs, StringLike const& rhs)
        {
            String s1("hello");
            std::string s2("hello");
            BOOST_TEST(!(s1 != s2));
            BOOST_TEST(!(s2 != s1));
        }

        // auto operator<=>(String const& lhs, StringLike const& rhs)
        {
            String s1("hello");
            std::string s2("hello");
            BOOST_TEST((s1 <=> s2) == std::strong_ordering::equal);
            BOOST_TEST((s2 <=> s1) == std::strong_ordering::equal);
        }

        // operator==(String const& lhs, String const& rhs)
        {
            String s1("hello");
            String s2("hello");
            BOOST_TEST(s1 == s2);
            BOOST_TEST(s2 == s1);
        }

        // operator!=(String const& lhs, String const& rhs)
        {
            String s1("hello");
            String s2("hello");
            BOOST_TEST(!(s1 != s2));
            BOOST_TEST(!(s2 != s1));
        }

        // auto operator<=>(String const& lhs, String const& rhs)
        {
            String s1("hello");
            String s2("hello");
            BOOST_TEST((s1 <=> s2) == std::strong_ordering::equal);
            BOOST_TEST((s2 <=> s1) == std::strong_ordering::equal);
        }

        // auto operator+(String const& lhs, String const& rhs)
        {
            String s1("hello");
            String s2("world");
            String s3 = s1 + s2;
            BOOST_TEST(s3 == "helloworld");
        }

        // operator+(S const& lhs, String const& rhs)
        {
            String s1("hello");
            std::string s2("world");
            String s3 = s2 + s1;
            BOOST_TEST(s3 == "worldhello");
        }

        // operator+(String const& lhs, S const& rhs)
        {
            String s1("hello");
            std::string s2("world");
            String s3 = s1 + s2;
            BOOST_TEST(s3 == "helloworld");
        }

        // fmt::formatter<clang::mrdox::dom::String>
        {
            String s("hello");
            BOOST_TEST(fmt::format("{}", s) == "hello");
        }
    }

    void
    array_test()
    {
        // Array()
        {
            Array a;
            BOOST_TEST(a.empty());
        }

        // Array(Array&&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2(std::move(a1));
            BOOST_TEST(a2.size() == 1);
        }

        // Array(Array const&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2(a1);
            BOOST_TEST(a2.size() == 1);
            BOOST_TEST(a1.size() == 1);
        }

        // Array(impl_type impl)
        {
            auto impl = std::make_shared<DefaultArrayImpl>();
            impl->emplace_back("hello");
            Array a(impl);
            a.emplace_back("world");
            BOOST_TEST(a.size() == 2);
        }

        // Array(storage_type)
        {
            Array::storage_type v;
            v.emplace_back("hello");
            Array a(v);
            a.emplace_back("world");
            BOOST_TEST(a.size() == 2);
        }

        // operator=(Array&&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2 = std::move(a1);
            BOOST_TEST(a2.size() == 1);
        }

        // operator=(Array const&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2 = a1;
            BOOST_TEST(a2.size() == 1);
            BOOST_TEST(a1.size() == 1);
        }

        // auto impl() const
        {
            {
                Array a;
                a.emplace_back("hello");
                BOOST_TEST(a.impl()->size() == 1);
            }

            {
                auto impl = std::make_shared<DefaultArrayImpl>();
                impl->emplace_back("hello");
                Array a(impl);
                a.emplace_back("world");
                BOOST_TEST(a.impl()->size() == 2);
                BOOST_TEST(impl->size() == 2);
            }
        }

        // char const* type_key()
        {
            Array a;
            BOOST_TEST(std::string_view(a.type_key()) == "Array");
        }

        // bool empty()
        {
            Array a;
            BOOST_TEST(a.empty());
            a.emplace_back("hello");
            BOOST_TEST(! a.empty());
        }

        // size_type size()
        {
            Array a;
            BOOST_TEST(a.empty());
            a.emplace_back("hello");
            BOOST_TEST(a.size() == 1);
        }

        // void set(size_type i, Value v);
        {
            Array a;
            a.emplace_back("hello");
            a.set(0, "world");
            BOOST_TEST(a.get(0) == "world");
        }

        // value_type get(size_type i)
        // value_type at(size_type i)
        {
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(a.get(0) == "hello");
            BOOST_TEST(a.at(0) == "hello");
        }

        // value_type front()
        {
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(a.front() == "hello");
        }

        // value_type back()
        {
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(a.back() == "hello");
        }

        // iterator begin()
        // iterator end()
        {
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(*a.begin() == "hello");
            BOOST_TEST(a.begin() != a.end());
        }

        // void push_back(value_type value)
        {
            Array a;
            a.push_back("hello");
            BOOST_TEST(a.size() == 1);
            BOOST_TEST(a.at(0) == "hello");
        }

        // template< class... Args >
        // void emplace_back(Args&&... args)
        {
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(a.size() == 1);
            BOOST_TEST(a.at(0) == "hello");
        }

        // friend Array operator+(Array const& lhs, Array const& rhs);
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2.emplace_back("world");
            Array a3 = a1 + a2;
            BOOST_TEST(a3.size() == 2);
            BOOST_TEST(a3.get(0) == "hello");
            BOOST_TEST(a3.get(1) == "world");
        }

        // template <std::convertible_to<Array> S>
        // friend auto operator+(S const& lhs, Array const& rhs)
        {
            Array a1;
            a1.emplace_back("hello");
            std::vector<Value> a2;
            a2.emplace_back("world");
            Array a3 = a2 + a1;
            BOOST_TEST(a3.size() == 2);
            BOOST_TEST(a3.get(0) == "world");
            BOOST_TEST(a3.get(1) == "hello");
            Array a4 = a1 + a2;
            BOOST_TEST(a4.size() == 2);
            BOOST_TEST(a4.get(0) == "hello");
            BOOST_TEST(a4.get(1) == "world");
        }

        // void swap(Array& other) noexcept
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2.emplace_back("world");
            a1.swap(a2);
            BOOST_TEST(a1.size() == 1);
            BOOST_TEST(a2.size() == 1);
            BOOST_TEST(a1.get(0) == "world");
            BOOST_TEST(a2.get(0) == "hello");
        }

        // friend void swap(Array& lhs, Array& rhs) noexcept
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2.emplace_back("world");
            swap(a1, a2);
            BOOST_TEST(a1.size() == 1);
            BOOST_TEST(a2.size() == 1);
            BOOST_TEST(a1.get(0) == "world");
            BOOST_TEST(a2.get(0) == "hello");
        }

        // operator==(Array const&, Array const&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2.emplace_back("hello");
            BOOST_TEST(a1 == a2);
            BOOST_TEST(a2 == a1);
            a1 = a2;
            BOOST_TEST(a1 == a2);
            BOOST_TEST(a2 == a1);
        }

        // operator<=>(Array const&, Array const&)
        {
            Array a1;
            a1.emplace_back("hello");
            Array a2;
            a2.emplace_back("hello");
            BOOST_TEST((a1 <=> a2) == std::strong_ordering::equal);
            BOOST_TEST((a2 <=> a1) == std::strong_ordering::equal);
        }

        // toString(Array const&);
        {
            // Behave same as JS:
            // x = ['hello']
            // x.toString() == 'hello'
            Array a;
            BOOST_TEST(toString(a).empty());
            a.emplace_back("hello");
            BOOST_TEST(toString(a) == "hello");
            a.emplace_back("world");
            BOOST_TEST(toString(a) == "hello,world");
        }
    }

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

        // char const* type_key() const noexcept
        {
            Object o;
            BOOST_TEST(std::string_view(o.type_key()) == "Object");
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
                BOOST_TEST(exp.error().reason() == "error");
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

        // type_key()
        {
            Function f([](dom::Value const& arg0)
            {
                return arg0;
            });
            BOOST_TEST(std::string_view(f.type_key()) == "Function");
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
                BOOST_TEST_THROWS(f("hello"), Exception);
                auto exp = f.call(newArray<DefaultArrayImpl>());
                BOOST_TEST(!exp);
                BOOST_TEST(exp.error().reason() == "error");
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

        // try_invoke(Args&&... args)
        // same as operator() but returns Expected instead of throwing
        {
            auto fn = [](dom::Value const& arg0) -> Expected<void>
            {
                BOOST_TEST(arg0 == "hello");
                return Unexpected(Error("error"));
            };
            Function f(fn);
            auto exp = f.try_invoke("hello");
            BOOST_TEST(!exp);
            BOOST_TEST(exp.error().reason() == "error");
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

    void
    value_test()
    {
        // Value() noexcept;
        {
            Value v;
            BOOST_TEST(v.isUndefined());
        }

        // Value(dom::Kind kind) noexcept;
        {
            // Undefined
            {
                Value v(Kind::Undefined);
                BOOST_TEST(v.isUndefined());
            }

            // Null
            {
                Value v(Kind::Null);
                BOOST_TEST(v.isNull());
            }

            // Boolean
            {
                Value v(Kind::Boolean);
                BOOST_TEST(v.isBoolean());
                BOOST_TEST(v == false);
            }

            // Integer
            {
                Value v(Kind::Integer);
                BOOST_TEST(v.isInteger());
                BOOST_TEST(v == 0);
            }

            // String
            {
                Value v(Kind::String);
                BOOST_TEST(v.isString());
                BOOST_TEST(v.empty());
            }

            // SafeString
            {
                Value v(Kind::SafeString);
                BOOST_TEST(v.isSafeString());
                BOOST_TEST(v.empty());
            }

            // Array
            {
                Value v(Kind::Array);
                BOOST_TEST(v.isArray());
                BOOST_TEST(v.empty());
            }

            // Object
            {
                Value v(Kind::Object);
                BOOST_TEST(v.isObject());
                BOOST_TEST(v.empty());
            }

            // Function
            {
                Value v(Kind::Function);
                BOOST_TEST(v.isFunction());
                BOOST_TEST(v().isUndefined());
            }
        }

        // Value(std::nullptr_t v) noexcept;
        {
            Value v(nullptr);
            BOOST_TEST(v.isNull());
        }

        // Value(std::int64_t v) noexcept;
        {
            Value v(123);
            BOOST_TEST(v.isInteger());
            BOOST_TEST(v == 123);
        }

        // Value(String str) noexcept;
        {
            Value v(String("hello"));
            BOOST_TEST(v.isString());
            BOOST_TEST(v == "hello");
        }

        // Value(Array arr) noexcept;
        {
            Array arr;
            arr.emplace_back("hello");
            Value v(arr);
            BOOST_TEST(v.isArray());
            BOOST_TEST(v.size() == 1);
        }

        // Value(Object obj) noexcept;
        {
            Object obj;
            obj.set("hello", "world");
            Value v(obj);
            BOOST_TEST(v.isObject());
            BOOST_TEST(v.size() == 1);
        }

        // Value(Function fn) noexcept;
        {
            Function fn([](Array const& args)
            {
                return args.get(0);
            });
            Value v(fn);
            BOOST_TEST(v.isFunction());
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(v(a) == "hello");
        }

        // Value(F const& f)
{
            Value v([](dom::Value const& arg0)
            {
                return arg0;
            });
            BOOST_TEST(v.isFunction());
            BOOST_TEST(v("hello") == "hello");
        }

        // Value(Boolean const& b)
        {
            Value v(true);
            BOOST_TEST(v.isBoolean());
            BOOST_TEST(v == true);
        }

        // Value(std::integral auto v)
        {
            {
                Value v(123);
                BOOST_TEST(v.isInteger());
                BOOST_TEST(v == 123);
            }

            {
                Value v(0);
                BOOST_TEST(v.isInteger());
                BOOST_TEST(v == 0);
            }
        }

        // Value(std::floating_point v)
        {
            {
                Value v(123.0);
                BOOST_TEST(v.isInteger());
                BOOST_TEST(v == 123);
            }

            {
                Value v(0.0);
                BOOST_TEST(v.isInteger());
                BOOST_TEST(v == 0);
            }
        }

        // Value(Enum v)
        {
            enum class E { A, B, C };
            Value v(E::A);
            BOOST_TEST(v.isInteger());
            BOOST_TEST(v == 0);
        }

        // Value(char const(&sz)[N])
        {
            Value v("hello");
            BOOST_TEST(v.isString());
            BOOST_TEST(v == "hello");
        }

        // Value(char const* s)
        {
            std::string s("hello");
            Value v(s.data());
            BOOST_TEST(v.isString());
            BOOST_TEST(v == "hello");
        }

        // Value(StringLike)
        {
            Value v(std::string("hello"));
            BOOST_TEST(v.isString());
            BOOST_TEST(v == "hello");
        }

        // Value(std::optional<T> const& opt)
        {
            {
                std::optional<dom::Value> opt;
                Value v(opt);
                BOOST_TEST(v.isUndefined());
            }

            {
                std::optional<dom::Value> opt(123);
                Value v(opt);
                BOOST_TEST(v.isInteger());
                BOOST_TEST(v == 123);
            }
        }

        // Value(Optional<T>)
        {
            {
                Optional<dom::Value> opt;
                Value v(opt);
                BOOST_TEST(v.isUndefined());
            }

            {
                Optional<dom::Value> opt(123);
                Value v(opt);
                BOOST_TEST(v.isInteger());
                BOOST_TEST(v == 123);
            }
        }

        // Value(Array::storage_type)
        {
            Array::storage_type v;
            v.emplace_back("hello");
            Value val(v);
            BOOST_TEST(val.isArray());
            BOOST_TEST(val.size() == 1);
        }

        // Value(Value const& other);
        {
            Value v1(123);
            Value v2(v1);
            BOOST_TEST(v2.isInteger());
            BOOST_TEST(v2 == 123);
            BOOST_TEST(v1.isInteger());
            BOOST_TEST(v1 == 123);
        }

        // Value(Value&& other) noexcept;
        {
            Value v1(123);
            Value v2(std::move(v1));
            BOOST_TEST(v2.isInteger());
            BOOST_TEST(v2 == 123);
        }

        // Value& operator=(Value&& other)
        {
            Value v1(123);
            Value v2;
            v2 = std::move(v1);
            BOOST_TEST(v2.isInteger());
            BOOST_TEST(v2 == 123);
        }

        // Value& operator=(Value const&)
        {
            Value v1(123);
            Value v2;
            v2 = v1;
            BOOST_TEST(v2.isInteger());
            BOOST_TEST(v2 == 123);
            BOOST_TEST(v1.isInteger());
            BOOST_TEST(v1 == 123);
        }

        // char const* type_key()
        {
            // Undefined
            {
                Value v(Kind::Undefined);
                BOOST_TEST(std::string_view(v.type_key()) == "undefined");
            }

            // Null
            {
                Value v(Kind::Null);
                BOOST_TEST(std::string_view(v.type_key()) == "null");
            }

            // Boolean
            {
                Value v(Kind::Boolean);
                BOOST_TEST(std::string_view(v.type_key()) == "boolean");
            }

            // Integer
            {
                Value v(Kind::Integer);
                BOOST_TEST(std::string_view(v.type_key()) == "integer");
            }

            // String
            {
                Value v(Kind::String);
                BOOST_TEST(std::string_view(v.type_key()) == "string");
            }

            // SafeString
            {
                Value v(Kind::SafeString);
                BOOST_TEST(std::string_view(v.type_key()) == "safeString");
            }

            // Array
            {
                Value v(Kind::Array);
                BOOST_TEST(std::string_view(v.type_key()) == "Array");
            }

            // Object
            {
                Value v(Kind::Object);
                BOOST_TEST(std::string_view(v.type_key()) == "Object");
            }

            // Function
            {
                Value v(Kind::Function);
                BOOST_TEST(std::string_view(v.type_key()) == "Function");
            }
        }

        // dom::Kind kind()
        // bool isUndefined()
        // bool isNull()
        // bool isBoolean()
        // bool isInteger()
        // bool isString()
        // bool isSafeString()
        // bool isArray()
        // bool isObject()
        // bool isFunction()
        {
            // Undefined
            {
                Value v(Kind::Undefined);
                BOOST_TEST(v.kind() == Kind::Undefined);
                BOOST_TEST(v.isUndefined());
            }

            // Null
            {
                Value v(Kind::Null);
                BOOST_TEST(v.kind() == Kind::Null);
                BOOST_TEST(v.isNull());
            }

            // Boolean
            {
                Value v(Kind::Boolean);
                BOOST_TEST(v.kind() == Kind::Boolean);
                BOOST_TEST(v.isBoolean());
            }

            // Integer
            {
                Value v(Kind::Integer);
                BOOST_TEST(v.kind() == Kind::Integer);
                BOOST_TEST(v.isInteger());
            }

            // String
            {
                Value v(Kind::String);
                BOOST_TEST(v.kind() == Kind::String);
                BOOST_TEST(v.isString());
            }

            // SafeString
            {
                Value v(Kind::SafeString);
                BOOST_TEST(v.kind() == Kind::SafeString);
                BOOST_TEST(v.isSafeString());
            }

            // Array
            {
                Value v(Kind::Array);
                BOOST_TEST(v.kind() == Kind::Array);
                BOOST_TEST(v.isArray());
            }

            // Object
            {
                Value v(Kind::Object);
                BOOST_TEST(v.kind() == Kind::Object);
                BOOST_TEST(v.isObject());
            }

            // Function
            {
                Value v(Kind::Function);
                BOOST_TEST(v.kind() == Kind::Function);
                BOOST_TEST(v.isFunction());
            }
        }

        // bool isTruthy()
        // operator bool()
        {
            // Undefined
            {
                Value v(Kind::Undefined);
                BOOST_TEST(! v.isTruthy());
                BOOST_TEST(! v);
            }

            // Null
            {
                Value v(Kind::Null);
                BOOST_TEST(! v.isTruthy());
                BOOST_TEST(! v);
            }

            // Boolean
            {
                Value v(Kind::Boolean);
                BOOST_TEST(! v.isTruthy());
                BOOST_TEST(! v);
                v = true;
                BOOST_TEST(v.isTruthy());
                BOOST_TEST(v);
            }

            // Integer
            {
                Value v(Kind::Integer);
                BOOST_TEST(! v.isTruthy());
                BOOST_TEST(! v);
                v = 123;
                BOOST_TEST(v.isTruthy());
                BOOST_TEST(v);
            }

            // String
            {
                Value v(Kind::String);
                BOOST_TEST(! v.isTruthy());
                BOOST_TEST(! v);
                v = "hello";
                BOOST_TEST(v.isTruthy());
                BOOST_TEST(v);
            }

            // SafeString
            {
                Value v(Kind::SafeString);
                BOOST_TEST(! v.isTruthy());
                BOOST_TEST(! v);
                v = "hello";
                BOOST_TEST(v.isTruthy());
                BOOST_TEST(v);
            }

            // Array
            {
                Value v(Kind::Array);
                BOOST_TEST(v.isTruthy());
                BOOST_TEST(v);
                v = Array();
                BOOST_TEST(v.isTruthy());
                BOOST_TEST(v);
            }

            // Object
            {
                Value v(Kind::Object);
                BOOST_TEST(v.isTruthy());
                BOOST_TEST(v);
                v = Object();
                BOOST_TEST(v.isTruthy());
                BOOST_TEST(v);
            }

            // Function
            {
                Value v(Kind::Function);
                BOOST_TEST(v.isTruthy());
                BOOST_TEST(v);
                v = Function();
                BOOST_TEST(v.isTruthy());
                BOOST_TEST(v);
            }
        }

        // getBool()
        {
            Value v(true);
            BOOST_TEST(v.getBool());
            v = false;
            BOOST_TEST(! v.getBool());
        }

        // getInteger()
        {
            Value v(123);
            BOOST_TEST(v.getInteger() == 123);
        }

        // getString()
        {
            Value v("hello");
            BOOST_TEST(v.getString() == "hello");
        }

        // getArray()
        {
            Array arr;
            arr.emplace_back("hello");
            Value v(arr);
            BOOST_TEST(v.getArray().size() == 1);
        }

        // getObject()
        {
            Object obj;
            obj.set("hello", "world");
            Value v(obj);
            BOOST_TEST(v.getObject().size() == 1);
        }

        // getFunction()
        {
            Function fn([](Array const& args)
            {
                return args.get(0);
            });
            Value v(fn);
            Array a;
            a.emplace_back("hello");
            BOOST_TEST(v.getFunction()(a) == "hello");
        }

        // get(std::string_view key)
        {
            // Object
            {
                Object obj;
                obj.set("hello", "world");
                Value v(obj);
                BOOST_TEST(v.get("hello") == "world");
            }

            // Array
            {
                Array arr;
                arr.emplace_back("hello");
                Value v(arr);
                BOOST_TEST(v.get("0") == "hello");
                BOOST_TEST(v.get("10").isUndefined());
                BOOST_TEST(v.get("hello").isUndefined());
            }

            // String
            {
                Value v("hello");
                BOOST_TEST(v.get("0") == "h");
                BOOST_TEST(v.get("10").isUndefined());
                BOOST_TEST(v.get("hello").isUndefined());
            }

            // Undefined
            {
                Value v(Kind::Undefined);
                BOOST_TEST(v.get("hello").isUndefined());
            }
        }

        // get(std::size_t i)
        {
            // Object
            {
                Object obj;
                obj.set("hello", "world");
                obj.set("1", "goodbye");
                Value v(obj);
                BOOST_TEST(v.get(0).isUndefined());
                BOOST_TEST(v.get(1) == "goodbye");
            }

            // Array
            {
                Array arr;
                arr.emplace_back("hello");
                Value v(arr);
                BOOST_TEST(v.get(0) == "hello");
                BOOST_TEST(v.get(1).isUndefined());
            }

            // String
            {
                Value v("hello");
                BOOST_TEST(v.get(0) == "h");
                BOOST_TEST(v.get(5).isUndefined());
            }

            // Undefined
            {
                Value v(Kind::Undefined);
                BOOST_TEST(v.get(0).isUndefined());
            }
        }

        // lookup
        {
            Object d;
            d.set("d", "e");
            Object obj;
            obj.set("a", "b");
            obj.set("c", d);
            Array arr;
            arr.emplace_back("hello");
            obj.set("arr", arr);
            Value v(obj);
            BOOST_TEST(v.lookup("a") == "b");
            BOOST_TEST(v.lookup("c").isObject());
            BOOST_TEST(v.lookup("c.d") == "e");
            BOOST_TEST(v.lookup("c.f").isUndefined());
            BOOST_TEST(v.lookup("arr.0") == "hello");
            BOOST_TEST(v.lookup("arr.1").isUndefined());
        }

        // set(String const& key, Value const& value)
        {
            // Object
            {
                Value v(Kind::Object);
                v.set("hello", "world");
                BOOST_TEST(v.get("hello") == "world");
            }

            // Array
            {
                Value v(Kind::Array);
                v.set("0", "hello");
                BOOST_TEST(v.get("0") == "hello");
            }
        }

        // exists(std::string_view key)
        {
            // Object
            {
                Object obj;
                obj.set("hello", "world");
                Value v(obj);
                BOOST_TEST(v.exists("hello"));
                BOOST_TEST(! v.exists("goodbye"));
            }

            // Array
            {
                Array arr;
                arr.emplace_back("hello");
                Value v(arr);
                BOOST_TEST(v.exists("0"));
                BOOST_TEST(! v.exists("1"));
            }

            // Undefined
            {
                Value v(Kind::Undefined);
                BOOST_TEST(! v.exists("hello"));
            }
        }

        // Value operator()
        {
            Value v(Kind::Function);
            BOOST_TEST(v().isUndefined());
        }

        // size()
        // empty()
        {
            // Undefined
            {
                Value v(Kind::Undefined);
                BOOST_TEST(v.size() == 0); // NOLINT(*-container-size-empty)
                BOOST_TEST(v.empty());
            }

            // Null
            {
                Value v(Kind::Null);
                BOOST_TEST(v.size() == 0); // NOLINT(*-container-size-empty)
                BOOST_TEST(v.empty());
            }

            // Boolean
            {
                Value v(Kind::Boolean);
                BOOST_TEST(v.size() == 1);
                BOOST_TEST(!v.empty());
            }

            // Integer
            {
                Value v(Kind::Integer);
                BOOST_TEST(v.size() == 1);
                BOOST_TEST(!v.empty());
            }

            // String
            {
                Value v(Kind::String);
                BOOST_TEST(v.size() == 0); // NOLINT(*-container-size-empty)
                BOOST_TEST(v.empty());
                v = "hello";
                BOOST_TEST(v.size() == 5);
                BOOST_TEST(!v.empty());
            }

            // SafeString
            {
                Value v(Kind::SafeString);
                BOOST_TEST(v.size() == 0); // NOLINT(*-container-size-empty)
                BOOST_TEST(v.empty());
                v = "hello";
                BOOST_TEST(v.size() == 5);
                BOOST_TEST(!v.empty());
            }

            // Array
            {
                Value v(Kind::Array);
                BOOST_TEST(v.size() == 0); // NOLINT(*-container-size-empty)
                BOOST_TEST(v.empty());
                v.getArray().push_back("hello");
                BOOST_TEST(v.size() == 1);
                BOOST_TEST(!v.empty());
            }

            // Object
            {
                Value v(Kind::Object);
                BOOST_TEST(v.size() == 0); // NOLINT(*-container-size-empty)
                BOOST_TEST(v.empty());
                v.set("hello", "world");
                BOOST_TEST(v.size() == 1);
                BOOST_TEST(!v.empty());
            }

            // Function
            {
                Value v(Kind::Function);
                BOOST_TEST(v.size() == 1);
                BOOST_TEST(!v.empty());
            }
        }

        // toString
        {
            // Undefined
            {
                Value v(Kind::Undefined);
                BOOST_TEST(toString(v) == "undefined");
            }

            // Null
            {
                Value v(Kind::Null);
                BOOST_TEST(toString(v) == "null");
            }

            // Boolean
            {
                Value v(Kind::Boolean);
                BOOST_TEST(toString(v) == "false");
                v = true;
                BOOST_TEST(toString(v) == "true");
            }

            // Integer
            {
                Value v(Kind::Integer);
                BOOST_TEST(toString(v) == "0");
                v = 123;
                BOOST_TEST(toString(v) == "123");
            }

            // String
            {
                Value v(Kind::String);
                BOOST_TEST(toString(v).empty());
                v = "hello";
                BOOST_TEST(toString(v) == "hello");
            }

            // SafeString
            {
                Value v(Kind::SafeString);
                BOOST_TEST(toString(v).empty());
                v = "hello";
                BOOST_TEST(toString(v) == "hello");
            }

            // Array
            {
                Value v(Kind::Array);
                BOOST_TEST(toString(v).empty());
                v.getArray().push_back("hello");
                BOOST_TEST(toString(v) == "hello");
                v.getArray().push_back("world");
                BOOST_TEST(toString(v) == "hello,world");
            }

            // Object
            {
                Value v(Kind::Object);
                BOOST_TEST(toString(v) == "[object Object]");
                v.getObject().set("hello", "world");
                BOOST_TEST(toString(v) == "[object Object]");
            }

            // Function
            {
                Value v(Kind::Function);
                BOOST_TEST(toString(v) == "[object Function]");
            }
        }

        // swap(Value& other)
        // swap(Value& lhs, Value& rhs)
        {
            Value v1(123);
            Value v2("hello");
            v1.swap(v2);
            BOOST_TEST(v1.isString());
            BOOST_TEST(v1 == "hello");
            BOOST_TEST(v2.isInteger());
            BOOST_TEST(v2 == 123);
            swap(v1, v2);
            BOOST_TEST(v1.isInteger());
            BOOST_TEST(v1 == 123);
            BOOST_TEST(v2.isString());
            BOOST_TEST(v2 == "hello");
        }

        // operator==(Value const&, Value const&)
        {
            // Types are not the same
            {
                Value v1(123);
                Value v2("hello");
                BOOST_TEST(v1 != v2);
                BOOST_TEST(v2 != v1);
            }

            // Undefined
            {
                Value v1(Kind::Undefined);
                Value v2(Kind::Undefined);
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
            }

            // Null
            {
                Value v1(Kind::Null);
                Value v2(Kind::Null);
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
            }

            // Boolean
            {
                Value v1(Kind::Boolean);
                Value v2(Kind::Boolean);
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
                v1 = true;
                BOOST_TEST(v1 != v2);
                BOOST_TEST(v2 != v1);
                v2 = true;
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
            }

            // Integer
            {
                Value v1(Kind::Integer);
                Value v2(Kind::Integer);
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
                v1 = 123;
                BOOST_TEST(v1 != v2);
                BOOST_TEST(v2 != v1);
                v2 = 123;
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
            }

            // String
            {
                Value v1(Kind::String);
                Value v2(Kind::String);
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
                v1 = "hello";
                BOOST_TEST(v1 != v2);
                BOOST_TEST(v2 != v1);
                v2 = "hello";
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
            }

            // SafeString
            {
                Value v1(Kind::SafeString);
                Value v2(Kind::SafeString);
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
                v1 = "hello";
                BOOST_TEST(v1 != v2);
                BOOST_TEST(v2 != v1);
                v2 = "hello";
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
            }

            // Array
            {
                Value v1(Kind::Array);
                Value v2(Kind::Array);
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
                v1 = v2;
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
            }

            // Object
            {
                Value v1(Kind::Object);
                Value v2(Kind::Object);
                BOOST_TEST(v1 != v2);
                BOOST_TEST(v2 != v1);
                v1 = v2;
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
            }

            // Function
            {
                Value v1(Kind::Function);
                Value v2(Kind::Function);
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
                v2 = Value([](){});
                BOOST_TEST(v1 != v2);
                BOOST_TEST(v2 != v1);
                v1 = v2;
                BOOST_TEST(v1 == v2);
                BOOST_TEST(v2 == v1);
            }
        }

        // operator<=>
        {
            // Types are not the same
            {
                Value v1(123);
                Value v2("hello");
                BOOST_TEST(v1 < v2);
                BOOST_TEST(v2 > v1);
            }

            // Undefined
            {
                Value v1(Kind::Undefined);
                Value v2(Kind::Undefined);
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equivalent);
            }

            // Null
            {
                Value v1(Kind::Null);
                Value v2(Kind::Null);
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equivalent);
            }

            // Boolean
            {
                Value v1(Kind::Boolean);
                Value v2(Kind::Boolean);
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equal);
                v1 = true;
                BOOST_TEST(v1 > v2);
                v2 = true;
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equal);
            }

            // Integer
            {
                Value v1(Kind::Integer);
                Value v2(Kind::Integer);
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equal);
                v1 = 123;
                BOOST_TEST(v1 > v2);
                v2 = 123;
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equal);
            }

            // String
            {
                Value v1(Kind::String);
                Value v2(Kind::String);
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equal);
                v1 = "hello";
                BOOST_TEST(v1 > v2);
                v2 = "hello";
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equal);
            }

            // Array
            {
                Value v1(Kind::Array);
                Value v2(Kind::Array);
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equal);
                v1 = v2;
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equal);
                Value v3(Kind::Array);
                v3.getArray().push_back("hello");
                BOOST_TEST(v1 < v3);
            }

            // Object
            {
                Value v1(Kind::Object);
                Value v2(Kind::Object);
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equivalent);
                v1 = v2;
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equal);
                Value v3(Kind::Object);
                v3.getObject().set("hello", "world");
                BOOST_TEST(v1 <=> v3 == std::strong_ordering::equivalent);
            }

            // Function
            {
                Value v1(Kind::Function);
                Value v2(Kind::Function);
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equivalent);
                v1 = v2;
                BOOST_TEST(v1 <=> v2 == std::strong_ordering::equal);
            }
        }

        // operator+
        {
            // Same types
            {
                // Integer
                {
                    Value v1(123);
                    Value v2(456);
                    BOOST_TEST((v1 + v2).isInteger());
                    BOOST_TEST((v1 + v2) == 579);
                }

                // String
                {
                    Value v1("hello");
                    Value v2("world");
                    BOOST_TEST((v1 + v2).isString());
                    BOOST_TEST((v1 + v2) == "helloworld");
                }

                // Array
                {
                    Value v1(Kind::Array);
                    v1.getArray().push_back("hello");
                    Value v2(Kind::Array);
                    v2.getArray().push_back("world");
                    BOOST_TEST((v1 + v2).isArray());
                    BOOST_TEST((v1 + v2).getArray().size() == 2);
                    BOOST_TEST((v1 + v2).getArray().get(0) == "hello");
                    BOOST_TEST((v1 + v2).getArray().get(1) == "world");
                }
            }

            // Arithmetic types (number + boolean)
            {
                Value v1(123);
                Value v2(true);
                BOOST_TEST((v1 + v2).isInteger());
                BOOST_TEST((v1 + v2) == 124);
                BOOST_TEST((v2 + v1).isInteger());
                BOOST_TEST((v2 + v1) == 124);
            }

            // coerce to strings
            {
                Value v1(123);
                Value v2("hello");
                BOOST_TEST((v1 + v2).isString());
                BOOST_TEST((v1 + v2) == "123hello");
                BOOST_TEST((v2 + v1).isString());
                BOOST_TEST((v2 + v1) == "hello123");
            }
        }

        // operator|| returns the first truthy value
        {
            Value v1(Kind::Undefined);
            Value v2(Kind::Undefined);
            BOOST_TEST((v1 || v2).isUndefined());
            v1 = 123;
            BOOST_TEST((v1 || v2).isInteger());
            BOOST_TEST((v1 || v2) == 123);
            v2 = 456;
            BOOST_TEST((v1 || v2).isInteger());
            BOOST_TEST((v1 || v2) == 123);
            v1 = 0;
            BOOST_TEST((v1 || v2).isInteger());
            BOOST_TEST((v1 || v2) == 456);
        }

        // operator&& returns the first falsy value
        {
            Value v1(Kind::Undefined);
            Value v2(Kind::Undefined);
            BOOST_TEST((v1 && v2).isUndefined());
            v1 = 123;
            BOOST_TEST((v1 && v2).isUndefined());
            v2 = 456;
            BOOST_TEST((v1 && v2).isInteger());
            BOOST_TEST((v1 && v2) == 456);
            v1 = 0;
            BOOST_TEST((v1 && v2).isInteger());
            BOOST_TEST((v1 && v2) == 0);
        }

        // JSON::stringify(dom::Value)
        {
            // Undefined
            {
                Value v(Kind::Undefined);
                BOOST_TEST(JSON::stringify(v) == "null");
            }

            // Null
            {
                Value v(Kind::Null);
                BOOST_TEST(JSON::stringify(v) == "null");
            }

            // Boolean
            {
                Value v(Kind::Boolean);
                BOOST_TEST(JSON::stringify(v) == "false");
                v = true;
                BOOST_TEST(JSON::stringify(v) == "true");
            }

            // Integer
            {
                Value v(Kind::Integer);
                BOOST_TEST(JSON::stringify(v) == "0");
                v = 123;
                BOOST_TEST(JSON::stringify(v) == "123");
            }

            // String
            {
                Value v(Kind::String);
                BOOST_TEST(JSON::stringify(v) == "\"\"");
                v = "hello";
                BOOST_TEST(JSON::stringify(v) == "\"hello\"");
            }

            // Array
            {
                Array arr;
                arr.emplace_back("hello");
                Value v(arr);
                BOOST_TEST(JSON::stringify(v) == "[\n    \"hello\"\n]");
            }

            // Object
            {
                Object obj;
                obj.set("hello", "world");
                obj.set("goodbye", "world");
                Value v(obj);
                BOOST_TEST(JSON::stringify(v) == "{\n    \"hello\": \"world\",\n    \"goodbye\": \"world\"\n}");
            }
        }
    }

    void run()
    {
        kind_test();
        string_test();
        array_test();
        object_test();
        function_test();
        value_test();
    }
};

TEST_SUITE(
    Dom_test,
    "clang.mrdox.dom");

} // dom
} // mrdox
} // clang

