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

struct Value_test
{
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

        // Value(Optional<T> const& opt)
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

        // char const* typeKey()
        {
            // Undefined
            {
                Value v(Kind::Undefined);
                BOOST_TEST(std::string_view(v.typeKey()) == "undefined");
            }

            // Null
            {
                Value v(Kind::Null);
                BOOST_TEST(std::string_view(v.typeKey()) == "null");
            }

            // Boolean
            {
                Value v(Kind::Boolean);
                BOOST_TEST(std::string_view(v.typeKey()) == "boolean");
            }

            // Integer
            {
                Value v(Kind::Integer);
                BOOST_TEST(std::string_view(v.typeKey()) == "integer");
            }

            // String
            {
                Value v(Kind::String);
                BOOST_TEST(std::string_view(v.typeKey()) == "string");
            }

            // SafeString
            {
                Value v(Kind::SafeString);
                BOOST_TEST(std::string_view(v.typeKey()) == "safeString");
            }

            // Array
            {
                Value v(Kind::Array);
                BOOST_TEST(std::string_view(v.typeKey()) == "Array");
            }

            // Object
            {
                Value v(Kind::Object);
                BOOST_TEST(std::string_view(v.typeKey()) == "Object");
            }

            // Function
            {
                Value v(Kind::Function);
                BOOST_TEST(std::string_view(v.typeKey()) == "Function");
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
        value_test();
    }
};

TEST_SUITE(
    Value_test,
    "clang.mrdocs.dom.Value");

} // dom
} // mrdocs
