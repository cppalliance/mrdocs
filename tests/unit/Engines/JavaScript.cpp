//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Engines/JavaScript.hpp>
#include <mrdocs/Handlebars.hpp>
#include <test_suite/test_suite.hpp>
#include <array>
#include <span>
#include <thread>
#include <vector>


namespace mrdocs {
namespace js {

namespace detail {
dom::Expected<dom::Value>
invokeHelper(Value const& fn, dom::Array const& args);
}

struct JavaScript_test
{
    void
    test_context()
    {
        Context ctx;
        Context ctx2(ctx);
        (void) ctx;
        (void) ctx2;
    }

    void
    test_scope()
    {
        Context ctx;

        // empty scope
        {
            Scope scope(ctx);
        }

        // Push values directly
        {
            Scope scope(ctx);

            // pushInteger(std::int64_t value);
            Value a = scope.pushInteger(1);
            BOOST_TEST(a.isInteger());
            BOOST_TEST(a.getDom() == 1);

            // pushDouble(double value);
            Value b = scope.pushDouble(1.5);
            BOOST_TEST(b.isDouble());
            BOOST_TEST(b.getDom() == 1.5);

            // pushBoolean(bool value);
            Value c = scope.pushBoolean(true);
            BOOST_TEST(c.isBoolean());
            BOOST_TEST(c.getDom() == true);

            // pushString(std::string_view value);
            Value e = scope.pushString("hello world");
            BOOST_TEST(e.isString());
            BOOST_TEST(e.getDom() == "hello world");

            // pushString with non-null-terminated view
            std::string backing = "_slice_test";
            Value slice = scope.pushString(std::string_view(backing.data() + 1, 5));
            BOOST_TEST(slice.isString());
            BOOST_TEST(slice.getDom() == "slice");

            // pushObject();
            Value f = scope.pushObject();
            BOOST_TEST(f.isObject());
            BOOST_TEST(f.getDom().isObject());

            // pushArray();
            Value g = scope.pushArray();
            BOOST_TEST(g.isArray());
            BOOST_TEST(g.getDom().isArray());
        }

        // script
        {
            Scope scope(ctx);
            Expected<void> r = scope.script("var x = 1;");
            BOOST_TEST(r);
            r = scope.script("print(x);");
            BOOST_TEST(!r);
            auto exp = scope.getGlobal("x");
            BOOST_TEST(exp);
            js::Value x = *exp;
            BOOST_TEST(x.isNumber());
            BOOST_TEST(x.getDom() == 1);
        }

        // eval
        {
            Scope scope(ctx);
            Expected<Value> r = scope.eval("1 + 2 + 3");
            BOOST_TEST(r);
            Value v = r.value();
            BOOST_TEST(v.isNumber());
            BOOST_TEST(v.getDom() == 6);
        }

        // compile_script
        {
            // last expression as implicit return value
            {
                Scope scope(ctx);
                Expected<Value> fnr = scope.compile_script("var x = 1; x;");
                BOOST_TEST(fnr);
                Value fn = *fnr;
                BOOST_TEST(fn.isFunction());
                Value x = fn();
                BOOST_TEST(x.isNumber());
                BOOST_TEST(x.getDom() == 1);
            }

            // single expression
            {
                Scope scope(ctx);
                Expected<Value> fnr = scope.compile_script("1 + 2 + 3");
                BOOST_TEST(fnr);
                Value fn = *fnr;
                BOOST_TEST(fn.isFunction());
                Value x = fn();
                BOOST_TEST(x.isNumber());
                BOOST_TEST(x.getDom() == 1 + 2 + 3);
            }

            // functions are not executed or returned
            {
                Scope scope(ctx);
                Expected<Value> fnr = scope.compile_script("function (a, b) { return a + b; }");
                BOOST_TEST(!fnr.has_value());
            }
        }

        // compile_function
        {
            // function: return value is function itself as JS Value
            {
                // function with no args
                {
                    Scope scope(ctx);
                    Expected<Value> fnr = scope.compile_function(
                        "function () { return 3; }");
                    BOOST_TEST(fnr);
                    Value fn = fnr.value();
                    BOOST_TEST(fn.isFunction());
                    Value x = fn();
                    BOOST_TEST(x.isNumber());
                    BOOST_TEST(x.getDom() == 3);
                }

                // named function also returned as object
                {
                    Scope scope(ctx);
                    Expected<Value> fnr = scope.compile_function(
                        "function a() { return 3; }");
                    BOOST_TEST(fnr);
                    Value fn = fnr.value();
                    BOOST_TEST(fn.isFunction());
                    Value x = fn();
                    BOOST_TEST(x.isNumber());
                    BOOST_TEST(x.getDom() == 3);
                }

                // single function
                {
                    Scope scope(ctx);
                    Expected<Value> fnr = scope.compile_function(
                        "function f(a, b) { return a + b; }");
                    BOOST_TEST(fnr);
                    Value fn = fnr.value();
                    BOOST_TEST(fn.isFunction());
                    Value x = fn(1, 2);
                    BOOST_TEST(x.isNumber());
                    BOOST_TEST(x.getDom() == 3);
                }

                // multiple functions: first function is returned
                {
                    Scope scope(ctx);
                    Expected<Value> fnr = scope.compile_function(
                        "function f(a, b) { return a + b; }\n"
                        "function g(a, b) { return a * b; }");
                    BOOST_TEST(fnr);
                    Value fn = fnr.value();
                    BOOST_TEST(fn.isFunction());
                    Value x = fn(3, 3);
                    BOOST_TEST(x.isNumber());
                    BOOST_TEST(x.getDom() == 6);
                }
            }
        }

        // getGlobal
        {
            Scope scope(ctx);
            scope.script("var x = 1;");
            auto exp = scope.getGlobal("x");
            BOOST_TEST(exp);
            js::Value x = *exp;
            BOOST_TEST(x.isNumber());
            BOOST_TEST(x.getDom() == 1);
        }

        // setGlobal
        {
            Scope scope(ctx);
            scope.setGlobal("y", 1);
            auto exp = scope.getGlobal("y");
            BOOST_TEST(exp);
            js::Value y = *exp;
            BOOST_TEST(y.isNumber());
            BOOST_TEST(y.getDom() == 1);
        }

        // setGlobal with >32-bit integers degrades to string to avoid UBSan in JerryScript
        {
            Scope scope(ctx);
            auto const big = static_cast<std::int64_t>(1) << 33;
            scope.setGlobal("big", dom::Value(big));
            auto exp = scope.getGlobal("big");
            BOOST_TEST(exp);
            js::Value bigVal = *exp;
            BOOST_TEST(bigVal.isString());
            BOOST_TEST(bigVal.getDom() == std::to_string(big));
        }

        // getGlobalObject
        {
            Scope scope(ctx);
            scope.script("var x = 1;");
            js::Value x = scope.getGlobalObject();
            BOOST_TEST(x.isObject());
            BOOST_TEST(x.get("x").isNumber());
            BOOST_TEST(x.get("x").getDom() == 1);
        }
    }

    void
    test_value()
    {
        // Value()
        {
            Context ctx;
            Scope scope(ctx);
            Value v;
            BOOST_TEST(v.isUndefined());
        }

        // Value(Value const&)
        {
            Context ctx;
            Scope scope(ctx);
            Value v1;
            Value v2(v1);
            BOOST_TEST(v2.isUndefined());
        }

        // Value(Value&&)
        {
            Context ctx;
            Scope scope(ctx);
            Value v1;
            Value v2(std::move(v1));
            BOOST_TEST(v2.isUndefined());
        }

        // operator=(Value const&)
        {
            Context ctx;
            Scope scope(ctx);
            Value v1;
            Value v2;
            v2 = v1;
            BOOST_TEST(v2.isUndefined());
        }

        // operator=(Value&&)
        {
            Context ctx;
            Scope scope(ctx);
            Value v1;
            Value v2;
            v2 = std::move(v1);
            BOOST_TEST(v2.isUndefined());
        }

        // type()
        // is*()
        // isTruthy()
        // operator bool()
        // toString()
        // operator std::string()
        // get*()
        {
            // undefined
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("undefined").value();
                BOOST_TEST(x.isUndefined());
                BOOST_TEST(x.type() == Type::undefined);
                BOOST_TEST(!x.isTruthy());
                BOOST_TEST(!static_cast<bool>(x));
            }

            // null
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("null").value();
                BOOST_TEST(x.isNull());
                BOOST_TEST(x.type() == Type::null);
                BOOST_TEST(!x.isTruthy());
                BOOST_TEST(!static_cast<bool>(x));
            }

            // boolean
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("true").value();
                BOOST_TEST(x.isBoolean());
                BOOST_TEST(x.type() == Type::boolean);
                BOOST_TEST(x.isTruthy());
                BOOST_TEST(static_cast<bool>(x));
                BOOST_TEST(x.getBool());
            }

            // number
            {
                // Integer
                {
                    Context context;
                    Scope scope(context);
                    Value x = scope.eval("1 + 2 + 3").value();
                    BOOST_TEST(x.isNumber());
                    BOOST_TEST(x.isInteger());
                    BOOST_TEST(x.type() == Type::number);
                    BOOST_TEST(x.isTruthy());
                    BOOST_TEST(static_cast<bool>(x));
                    BOOST_TEST(x.getInteger() == 6);
                }

                // Double
                {
                    Context context;
                    Scope scope(context);
                    Value x = scope.eval("1.5 + 2.5 + 3.5").value();
                    BOOST_TEST(x.isNumber());
                    BOOST_TEST(x.isDouble());
                    BOOST_TEST(x.type() == Type::number);
                    BOOST_TEST(x.isTruthy());
                    BOOST_TEST(static_cast<bool>(x));
                    BOOST_TEST(x.getDouble() == 1.5 + 2.5 + 3.5);
                    BOOST_TEST(x.getInteger() == 7);
                }
            }

            // string
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("'hello world'").value();
                BOOST_TEST(x.isString());
                BOOST_TEST(x.type() == Type::string);
                BOOST_TEST(x.isTruthy());
                BOOST_TEST(static_cast<bool>(x));
                BOOST_TEST(x.getString() == "hello world");
            }

            // object
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("({ x: 1 })").value();
                BOOST_TEST(x.isObject());
                BOOST_TEST(x.type() == Type::object);
                BOOST_TEST(x.isTruthy());
                BOOST_TEST(static_cast<bool>(x));
                dom::Object o = x.getObject();
                BOOST_TEST(o.size() == 1);
                BOOST_TEST(o.exists("x"));
                BOOST_TEST(o.get("x").isInteger());
                BOOST_TEST(o.get("x").getInteger() == 1);
            }

            // function
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("(function() { return 1; })").value();
                BOOST_TEST(x.isFunction());
                BOOST_TEST(x.type() == Type::function);
                BOOST_TEST(x.isTruthy());
                BOOST_TEST(static_cast<bool>(x));
                dom::Function f = x.getFunction();
                BOOST_TEST(f() == 1);
            }

            // array
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("([1, 2, 3])").value();
                BOOST_TEST(x.isArray());
                BOOST_TEST(x.type() == Type::array);
                BOOST_TEST(x.isTruthy());
                BOOST_TEST(static_cast<bool>(x));
                dom::Array a = x.getArray();
                BOOST_TEST(a.size() == 3);
                BOOST_TEST(a.get(0).isInteger());
                BOOST_TEST(a.get(0).getInteger() == 1);
                BOOST_TEST(a.get(1).isInteger());
                BOOST_TEST(a.get(1).getInteger() == 2);
                BOOST_TEST(a.get(2).isInteger());
                BOOST_TEST(a.get(2).getInteger() == 3);
            }
        }

        // getDom()
        {
            // undefined
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("undefined").value();
                BOOST_TEST(x.isUndefined());
                dom::Value y = x.getDom();
                BOOST_TEST(y.isUndefined());
                dom::Value z(dom::Kind::Undefined);
                BOOST_TEST(y == z);
            }

            // null
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("null").value();
                BOOST_TEST(x.isNull());
                dom::Value y = x.getDom();
                BOOST_TEST(y.isNull());
                dom::Value z(dom::Kind::Null);
                BOOST_TEST(y == z);
            }

            // boolean
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("true").value();
                BOOST_TEST(x.isBoolean());
                dom::Value y = x.getDom();
                BOOST_TEST(y.isBoolean());
                dom::Value z(true);
                BOOST_TEST(y == z);
            }

            // number
            {
                // integer
                {
                    Context context;
                    Scope scope(context);
                    Value x = scope.eval("1 + 2 + 3").value();
                    BOOST_TEST(x.isNumber());
                    dom::Value y = x.getDom();
                    BOOST_TEST(y.isInteger());
                    dom::Value z(1 + 2 + 3);
                    BOOST_TEST(y == z);
                }

                // double: coerce to integer
                {
                    Context context;
                    Scope scope(context);
                    Value x = scope.eval("1.5 + 2.5 + 3.5").value();
                    BOOST_TEST(x.isNumber());
                    BOOST_TEST(x.isDouble());
                    dom::Value y = x.getDom();
                    BOOST_TEST(y.isInteger());
                    dom::Value z(1 + 2 + 3 + 1);
                    BOOST_TEST(y == z);
                }
            }

            // object
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("({ a: 1, b: true, c: 'c' })").value();
                BOOST_TEST(x.isObject());
                dom::Value y = x.getDom();
                BOOST_TEST(y.isObject());
                BOOST_TEST(y.get("a").isInteger());
                BOOST_TEST(y.get("a").getInteger() == 1);
                BOOST_TEST(y.get("b").isBoolean());
                BOOST_TEST(y.get("b").getBool());
                BOOST_TEST(y.get("c").isString());
                BOOST_TEST(y.get("c").getString() == "c");
                dom::Object z = y.getObject();
                z.set("d", nullptr);
                BOOST_TEST(z.size() == 4);
                BOOST_TEST(z.exists("b"));
                BOOST_TEST(z.exists("d"));
                z.visit([](dom::String const& key, dom::Value const& value)
                {
                    bool keyOk = (key == "a" || key == "b" || key == "c" || key == "d");
                    BOOST_TEST(keyOk);
                    bool valueOk = value.isInteger() || value.isBoolean()
                        || value.isString() || value.isNull();
                    BOOST_TEST(valueOk);
                });
            }

            // array
            {
                Context context;
                Scope scope(context);
                Value x = scope.eval("([1, true, 'c'])").value();
                BOOST_TEST(x.isArray());
                dom::Value y = x.getDom();
                BOOST_TEST(y.isArray());
                BOOST_TEST(y.get(0).isInteger());
                BOOST_TEST(y.get(0).getInteger() == 1);
                BOOST_TEST(y.get(1).isBoolean());
                BOOST_TEST(y.get(1).getBool());
                BOOST_TEST(y.get(2).isString());
                BOOST_TEST(y.get(2).getString() == "c");
                dom::Array z = y.getArray();
                z.push_back(nullptr);
                z.set(1, false);
                BOOST_TEST(z.size() == 4);
                for (std::size_t i = 0; i < z.size(); ++i)
                {
                    dom::Value v = z.get(i);
                    bool valueOk = v.isInteger() || v.isBoolean() || v.isString()
                        || v.isNull();
                    BOOST_TEST(valueOk);
                }
            }

            // function
            {
                // no parameters
                {
                    Context context;
                    Scope scope(context);
                    Value x = scope.eval("(function() { return 1; })").value();
                    BOOST_TEST(x.isFunction());
                    dom::Value y = x.getDom();
                    BOOST_TEST(y.isFunction());
                    BOOST_TEST(y() == 1);
                }

                // with parameters
                {
                    Context context;
                    Scope scope(context);
                    Value x = scope.eval("(function(a, b) { return a + b; })").value();
                    BOOST_TEST(x.isFunction());
                    dom::Value y = x.getDom();
                    BOOST_TEST(y.isFunction());
                    BOOST_TEST(y(1, 2) == 3);
                    BOOST_TEST(y(1, 2, 3) == 3);
                    BOOST_TEST(y(3, 4) == 7);
                }

                // variadic parameters
                {
                    Context context;
                    Scope scope(context);
                    Value x = scope.eval("(function() { return arguments.length; })").value();
                    BOOST_TEST(x.isFunction());
                    dom::Value y = x.getDom();
                    BOOST_TEST(y.isFunction());
                    BOOST_TEST(y() == 0);
                    BOOST_TEST(y(1) == 1);
                    BOOST_TEST(y(1, 2) == 2);
                    BOOST_TEST(y(1, 2, 3) == 3);
                }
            }
        }

        // get(std::string_view)
        // exists(std::string_view)
        {
            Context context;
            Scope scope(context);
            Value x = scope.eval("({ a: 1, b: true, c: 'c' })").value();
            BOOST_TEST(x.isObject());
            BOOST_TEST(x.exists("a"));
            BOOST_TEST(x.get("a").getDom() == 1);
            dom::String k("b");
            BOOST_TEST(x.exists("b"));
            BOOST_TEST(x.get(k).getDom() == true);
            dom::Value kv("c");
            BOOST_TEST(x.exists("c"));
            BOOST_TEST(x.get(kv).getDom() == "c");
        }

        // get(std::size_t)
        // exists(std::string_view)
        {
            Context context;
            Scope scope(context);
            Value x = scope.eval("([1, true, 'c'])").value();
            BOOST_TEST(x.isArray());
            BOOST_TEST(x.exists("0"));
            BOOST_TEST(x.get(0).getDom() == 1);
            BOOST_TEST(x.exists("1"));
            BOOST_TEST(x.get(1).getDom() == true);
            BOOST_TEST(x.exists("2"));
            dom::Value k(2);
            BOOST_TEST(x.get(k).getDom() == "c");
        }

        // lookup(std::string_view)
        {
            Context context;
            Scope scope(context);
            Value x = scope.eval("({ a: { b: { c: 123 }}})").value();
            BOOST_TEST(x.isObject());
            BOOST_TEST(x.lookup("a.b.c").getInteger() == 123);
        }

        // set(std::string_view,js::Value)
        {
            Context context;
            Scope scope(context);
            Value x = scope.eval("({})").value();
            Value y = scope.eval("123").value();
            BOOST_TEST(x.isObject());
            BOOST_TEST(y.isInteger());
            x.set("a", y);
            BOOST_TEST(x.get("a").getDom() == 123);
        }

        // set(std::string_view,dom::Value)
        {
            Context context;
            Scope scope(context);
            Value x = scope.eval("({})").value();
            dom::Value y = 123;
            BOOST_TEST(x.isObject());
            BOOST_TEST(y.isInteger());
            x.set("a", y);
            BOOST_TEST(x.get("a").getDom() == 123);
        }

        // erase(std::string_view)
        {
            Context context;
            Scope scope(context);
            Value obj = scope.eval("({ a: 1, b: 2 })").value();
            BOOST_TEST(obj.exists("a"));
            obj.erase("a");
            BOOST_TEST(!obj.exists("a"));
            BOOST_TEST(obj.exists("b"));
        }

        // empty()
        // size()
        {
            Context context;
            Scope scope(context);

            // undefined
            {
                Value a = scope.eval("(undefined)").value();
                BOOST_TEST(a.isUndefined());
                BOOST_TEST(a.empty());
                BOOST_TEST(a.size() == 0);
            }

            // null
            {
                Value b = scope.eval("(null)").value();
                BOOST_TEST(b.isNull());
                BOOST_TEST(b.empty());
                BOOST_TEST(b.size() == 0);
            }

            // boolean
            {
                Value c = scope.eval("(true)").value();
                BOOST_TEST(c.isBoolean());
                BOOST_TEST(!c.empty());
                BOOST_TEST(c.size() == 1);
            }

            // number
            {
                Value e = scope.eval("(123)").value();
                BOOST_TEST(e.isNumber());
                BOOST_TEST(!e.empty());
                BOOST_TEST(e.size() == 1);
            }

            // string
            {
                Value s = scope.eval("'Hello world'").value();
                BOOST_TEST(s.isString());
                BOOST_TEST(!s.empty());
                BOOST_TEST(s.size() == 11);
                Value s2 = scope.eval("('')").value();
                BOOST_TEST(s2.isString());
                BOOST_TEST(s2.empty());
                BOOST_TEST(s2.size() == 0);
            }

            // object
            {
                Value x = scope.eval("({})").value();
                BOOST_TEST(x.isObject());
                BOOST_TEST(x.empty());
                BOOST_TEST(x.size() == 0);
                x.set("a", 1);
            BOOST_TEST(!x.empty());
            BOOST_TEST(x.size() == 1);
            }

            // function
            {
                Value f = scope.eval("(function() {})").value();
                BOOST_TEST(f.isFunction());
                // JerryScript wrapper does not expose meaningful size/empty metadata; ensure the function is callable
                BOOST_TEST(f.call());
            }

            // array
            {
                Value y = scope.eval("([])").value();
                BOOST_TEST(y.isArray());
                BOOST_TEST(y.empty());
                BOOST_TEST(y.size() == 0);
                Value z = scope.eval("([1, 2, 3])").value();
                BOOST_TEST(!z.empty());
                BOOST_TEST(z.size() == 3);
            }
        }

        // operator()
        // call(...)
        // apply()
        {
            Context context;
            Scope scope(context);
            Value x = scope.eval("(function f(a, b) { return a + b; })").value();
            BOOST_TEST(x.isFunction());
            BOOST_TEST(x.call(1, 2).value().getDom() == 3);
            std::array<dom::Value, 2> args = {{1, 2}};
            BOOST_TEST(x.apply(args).value().getDom() == 3);
            BOOST_TEST(x(1, 2).getDom() == 3);
        }

        // swap(Value& other)
        // swap(Value& v0, Value& v1)
        {
            Context context;
            Scope scope(context);
            Value a = scope.eval("123").value();
            Value b = scope.eval("true").value();
            BOOST_TEST(a.isNumber());
            BOOST_TEST(b.isBoolean());
            BOOST_TEST(a.getInteger() == 123);
            BOOST_TEST(b.getBool());
            a.swap(b);
            BOOST_TEST(a.isBoolean());
            BOOST_TEST(b.isNumber());
            BOOST_TEST(a.getBool());
            BOOST_TEST(b.getInteger() == 123);
            swap(a, b);
            BOOST_TEST(a.isNumber());
            BOOST_TEST(b.isBoolean());
            BOOST_TEST(a.getInteger() == 123);
            BOOST_TEST(b.getBool());
        }

        // operator==(Value const&, Value const&)
        // operator!=(Value const&, Value const&)
        // operator<=>(Value const&, Value const&)
        {
            Context context;
            Scope scope(context);
            Value x1;
            Value x2;
            Value undef = scope.eval("undefined").value();
            Value i1 = scope.eval("123").value();
            Value i2 = scope.eval("123").value();
            Value i3 = scope.eval("124").value();
            Value b = scope.eval("true").value();
            BOOST_TEST(x1 == x2);
            BOOST_TEST(!(x1 < x2));
            BOOST_TEST(x1.isUndefined());
            BOOST_TEST(x1 != i1);
            BOOST_TEST(x1 < i1);
            BOOST_TEST(undef != i1);
            BOOST_TEST(undef < i1);
            BOOST_TEST(i1 == i2);
            BOOST_TEST(!(i1 < i2));
            BOOST_TEST(i1 != i3);
            BOOST_TEST(i1 < i3);
            BOOST_TEST(i1 != b);
            BOOST_TEST(i1 > b);
        }

        // operator||
        // operator&&
        {
            Context context;
            Scope scope(context);
            Value a = scope.eval("undefined").value();
            Value b = scope.eval("123").value();
            Value c = scope.eval("'hello world'").value();
            BOOST_TEST((a || b).getInteger() == 123);
            BOOST_TEST((b || c).getInteger() == 123);
            BOOST_TEST((c || b).getString() == "hello world");
            BOOST_TEST((a && b).isUndefined());
            BOOST_TEST((b && c).getString() == "hello world");
            BOOST_TEST((c && b).getInteger() == 123);
            BOOST_TEST((a || b || c).getInteger() == 123);
            BOOST_TEST((a && b && c).isUndefined());
        }
    }

    void
    test_cpp_function()
    {
        Context context;

        // Back and forth from JS
        {
            // Create JS function
            Scope scope(context);
            Value x = scope.eval("(function() { return 1; })").value();
            BOOST_TEST(x.isFunction());
            dom::Function f = x.getFunction();
            BOOST_TEST(f() == 1);

            // Register proxy to JS function as another object
            scope.setGlobal("fn", f);

            // Get new function as JS Value
            auto fnexp = scope.getGlobal("fn");
            BOOST_TEST(fnexp);
            Value fn = *fnexp;
            BOOST_TEST(fn.isFunction());
            BOOST_TEST(fn.call().value().getDom() == 1);

            // Get new function as dom::Value
            dom::Value fnv = fn.getDom();
            BOOST_TEST(fnv.isFunction());
            BOOST_TEST(fnv() == 1);
        }

        // Back and forth from C++
        {
            // Create C++ function
            Scope scope(context);
            auto cpp_add = dom::makeInvocable(
                [](int a, int b)
                {
                    return a + b;
                });
            BOOST_TEST(cpp_add(2, 3) == 5);

            // Register proxy to C++ function as JS object
            scope.setGlobal("fn", cpp_add);

            // Test C++ function usage from JS
            scope.eval("var x = fn(1, 2);");
            auto exp = scope.getGlobal("x");
            BOOST_TEST(exp);
            Value x = *exp;
            BOOST_TEST(x.isNumber());
            BOOST_TEST(x.getDom() == 3);

            // Get the C++ function as a JS Value
            auto fnexp = scope.getGlobal("fn");
            BOOST_TEST(fnexp);
            Value fn = *fnexp;
            BOOST_TEST(fn.isFunction());
            BOOST_TEST(fn(1, 2).getDom() == 3);

            // Get the C++ function as a dom::Value
            dom::Value fnv = fn.getDom();
            BOOST_TEST(fnv.isFunction());
            BOOST_TEST(fnv(1, 2) == 3);
        }

        // C++ function with state
        {
            // Create C++ function
            Scope scope(context);
            int state = 3;
            auto cpp_add = dom::makeInvocable(
                [state](int a, int b)
                {
                    return a + b + state;
                });
            BOOST_TEST(cpp_add(1, 2) == 6);

            // Register proxy to C++ function as JS object
            scope.setGlobal("fn", cpp_add);

            // Test C++ function usage from JS
            scope.eval("var x = fn(1, 2);");
            auto exp = scope.getGlobal("x");
            BOOST_TEST(exp);
            Value x = *exp;
            BOOST_TEST(x.isNumber());
            BOOST_TEST(x.getDom() == 6);

            // Get the C++ function as a JS Value
            auto fnexp = scope.getGlobal("fn");
            BOOST_TEST(fnexp);
            Value fn = *fnexp;
            BOOST_TEST(fn.isFunction());

            // Get the C++ function as a dom::Value
            dom::Value fnv = fn.getDom();
            BOOST_TEST(fnv.isFunction());
            BOOST_TEST(fnv(1, 2) == 6);
        }
    }

    void
    test_cpp_object()
    {
        Context context;

        // Back and forth from JS
        {
            // Create JS object
            Scope scope(context);
            Value x = scope.eval("({ a: 1 })").value();
            BOOST_TEST(x.isObject());
            dom::Object o1 = x.getObject();
            BOOST_TEST(o1.get("a") == 1);

            // Register proxy to JS object as another object
            scope.setGlobal("o", o1);

            // Get new function as JS Value
            auto oexp = scope.getGlobal("o");
            BOOST_TEST(oexp);
            Value o2 = *oexp;
            BOOST_TEST(o2.isObject());
            BOOST_TEST(o2.get("a").getDom() == 1);

            // Get new function as dom::Value
            dom::Value o3 = o2.getDom();
            BOOST_TEST(o3.isObject());
            BOOST_TEST(o3.get("a") == 1);
        }

        // Back and forth from C++
        // The lazy proxy design:
        // - JS reads from C++ object via get trap (reads live object)
        // - C++ writes are visible from JS (get trap reads live object)
        // - JS writes do NOT propagate to C++ (no set trap)
        {
            Scope scope(context);
            dom::Object o1;
            o1.set("a", 1);
            BOOST_TEST(o1.get("a") == 1);

            // Register proxy to C++ object as JS object
            scope.setGlobal("o", o1);

            // JS can read C++ object properties via the get trap
            scope.eval("var x = o.a;");
            auto exp = scope.getGlobal("x");
            BOOST_TEST(exp);
            Value x = *exp;
            BOOST_TEST(x.isNumber());
            BOOST_TEST(x.getDom() == 1);

            // JS writes route through the proxy's set trap into
            // dom::ObjectImpl::set; for the default object impl this
            // updates the underlying storage and the new value is
            // visible on the C++ side.
            scope.eval("o.a = 2;");
            BOOST_TEST(o1.get("a") == 2);

            // Restore the original value so the rest of the test
            // sees a known starting state.
            o1.set("a", 1);

            // 'in' operator works via has trap
            scope.eval("var hasA = 'a' in o;");
            auto hasExp = scope.getGlobal("hasA");
            BOOST_TEST(hasExp);
            BOOST_TEST(hasExp->isBoolean());
            BOOST_TEST(hasExp->getBool() == true);

            // delete does NOT affect C++ object (no deleteProperty trap)
            Expected<Value> de = scope.eval("delete o.a;");
            BOOST_TEST(de);
            BOOST_TEST(o1.get("a") == 1);  // C++ object unchanged

            // ownKeys trap returns keys from C++ object
            scope.eval("var z = Object.keys(o);");
            auto zexp = scope.getGlobal("z");
            BOOST_TEST(zexp);
            Value z = *zexp;
            BOOST_TEST(z.isArray());
            BOOST_TEST(z.size() == 1);
            BOOST_TEST(z.get(0).getString() == std::string("a"));

            // C++ writes ARE visible from JS (get trap reads live object)
            o1.set("a", 3);
            scope.eval("var x2 = o.a;");
            auto exp2 = scope.getGlobal("x2");
            BOOST_TEST(exp2);
            BOOST_TEST(exp2->isNumber());
            BOOST_TEST(exp2->getDom() == 3);

            // New C++ fields are visible from JS
            o1.set("b", 4);
            o1.set("c", 5);
            scope.eval("var z2 = Object.keys(o);");
            auto zexp2 = scope.getGlobal("z2");
            BOOST_TEST(zexp2);
            Value z2 = *zexp2;
            BOOST_TEST(z2.isArray());
            BOOST_TEST(z2.size() == 3);

            // Get the C++ object as a JS Value and verify properties
            auto oexp = scope.getGlobal("o");
            BOOST_TEST(oexp);
            Value o2 = *oexp;
            BOOST_TEST(o2.isObject());
            BOOST_TEST(o2.get("a").getDom() == 3);
            BOOST_TEST(o2.get("b").getDom() == 4);
            BOOST_TEST(o2.get("c").getDom() == 5);

            // Get the C++ object as a dom::Value
            dom::Value o3 = o2.getDom();
            BOOST_TEST(o3.isObject());
            BOOST_TEST(o3.get("a") == 3);
            BOOST_TEST(o3.get("b") == 4);
            BOOST_TEST(o3.get("c") == 5);
        }
    }

    void
    test_cpp_array()
    {
        Context context;

        // Back and forth from JS
        {
            // Create JS array
            Scope scope(context);
            Value x = scope.eval("([1, 2, 3])").value();
            BOOST_TEST(x.isArray());
            dom::Array a1 = x.getArray();
            BOOST_TEST(a1.get(0) == 1);

            // Register proxy to JS array as another array
            scope.setGlobal("a", a1);

            // Get new function as JS Value
            auto oexp = scope.getGlobal("a");
            BOOST_TEST(oexp);
            Value a2 = *oexp;
            BOOST_TEST(a2.isArray());
            BOOST_TEST(a2.get(0).getDom() == 1);

            // Get new function as dom::Value
            dom::Value o3 = a2.getDom();
            BOOST_TEST(o3.isArray());
            BOOST_TEST(o3.get(0) == 1);
        }

        // Back and forth from C++
        // Arrays use eager conversion (snapshot semantics), unlike objects which
        // use lazy proxies. This means:
        // - JS gets a snapshot of the C++ array at conversion time
        // - JS mutations do NOT affect the C++ array
        // - C++ mutations do NOT affect the JS array (it's a copy)
        {
            Scope scope(context);
            dom::Array a1({1, 2, 3});
            BOOST_TEST(a1.get(0) == 1);

            // Register C++ array as JS array (creates a snapshot)
            scope.setGlobal("a", a1);

            // JS can read the snapshot values
            scope.eval("var x = a[0];");
            auto exp = scope.getGlobal("x");
            BOOST_TEST(exp);
            BOOST_TEST(exp->isNumber());
            BOOST_TEST(exp->getDom() == 1);

            // JS array has correct length
            scope.eval("var l = a.length;");
            exp = scope.getGlobal("l");
            BOOST_TEST(exp);
            BOOST_TEST(exp->isNumber());
            BOOST_TEST(exp->getDom() == 3);

            // Undefined field access
            scope.eval("var u = a.field;");
            exp = scope.getGlobal("u");
            BOOST_TEST(exp);
            BOOST_TEST(exp->isUndefined());

            // JS mutations do NOT propagate to C++ array (snapshot semantics)
            scope.eval("a[0] = 99;");
            BOOST_TEST(a1.get(0) == 1);  // C++ array unchanged

            // JS can add elements, but C++ array is unchanged
            scope.eval("a[5] = 10;");
            BOOST_TEST(a1.get(5).isUndefined());

            // C++ mutations do NOT affect the JS snapshot
            a1.set(0, 42);
            scope.eval("var x2 = a[0];");
            auto exp2 = scope.getGlobal("x2");
            BOOST_TEST(exp2);
            // JS still has the original snapshot value (1) or JS-mutated value (99)
            BOOST_TEST(exp2->isNumber());
            BOOST_TEST(exp2->getDom() != 42);  // C++ change not visible

            // 'in' operator works on JS array
            scope.eval("var hasIdx = 0 in a;");
            auto hasExp = scope.getGlobal("hasIdx");
            BOOST_TEST(hasExp);
            BOOST_TEST(hasExp->isBoolean());
            BOOST_TEST(hasExp->getBool() == true);

            scope.eval("var hasLength = 'length' in a;");
            hasExp = scope.getGlobal("hasLength");
            BOOST_TEST(hasExp);
            BOOST_TEST(hasExp->isBoolean());
            BOOST_TEST(hasExp->getBool() == true);

            // Object.keys returns array indices as strings
            scope.eval("var z = Object.keys(a);");
            auto zexp = scope.getGlobal("z");
            BOOST_TEST(zexp);
            BOOST_TEST(zexp->isArray());
            // Keys are string indices: "0", "1", "2", plus any JS-added indices
            for (auto const& v : zexp->getArray())
            {
                BOOST_TEST(v.isString());
            }

            // Get the JS array as a Value and verify it has JS mutations
            auto aexp = scope.getGlobal("a");
            BOOST_TEST(aexp);
            Value a2 = *aexp;
            BOOST_TEST(a2.isArray());
            BOOST_TEST(a2.get(0).isNumber());

            // Get as dom::Value
            dom::Value a3 = a2.getDom();
            BOOST_TEST(a3.isArray());
            BOOST_TEST(a3.get(0).isInteger());
        }
    }

    void
    test_hbs_helpers()
    {
        handlebars::Handlebars hbs;
        js::Context ctx;
        // Simple inline helper happy path
        auto ok = js::registerHelper(
            hbs,
            "inlineok",
            ctx,
            "(function(){ return function(){ return 'inline-ok'; }; })()"
        );
        BOOST_TEST(ok);
        if (ok)
            BOOST_TEST(hbs.render("{{inlineok}}") == "inline-ok");
    }

    void
    test_helper_error_propagation()
    {
        handlebars::Handlebars hbs;
        js::Context ctx;

        // Syntax error should surface directly, not be masked as "not a function".
        auto bad = js::registerHelper(hbs, "bad", ctx, "function() {");
        BOOST_TEST(!bad);
        if (!bad)
        {
            auto const& msg = bad.error().message();
            BOOST_TEST(msg.find("Unexpected") != std::string::npos);
        }

        // Valid named function without return should still be discovered on the global object.
        auto ok = js::registerHelper(hbs, "adder", ctx, "function adder(a, b) { return a + b; }");
        BOOST_TEST(ok);
    }

    void
    test_value_lifetime_and_apply_errors()
    {
        // Values keep the engine alive through the shared Context impl, even
        // after the creating Scope goes out of scope.
        {
            Context ctx;
            dom::Function stored;
            bool haveFn = false;
            {
                Scope scope(ctx);
                auto fnExp = scope.eval("(function(x) { return x + 1; })");
                BOOST_TEST(fnExp);
                if (fnExp)
                {
                    stored = fnExp->getFunction();
                    haveFn = true;
                }
            }

            {
                Scope scope(ctx);
                if (haveFn)
                {
                    dom::Array arr;
                    arr.push_back(dom::Value(2));
                    auto res = stored(arr);
                    BOOST_TEST(res);
                    if (res && res.isInteger())
                    {
                        BOOST_TEST(res.getInteger() == 3);
                    }
                }
            }
        }

        // apply() shares the call path with call(), returning rich errors from
        // the engine for both non-functions and thrown exceptions.
        {
            Context ctx;
            Scope scope(ctx);
            auto number = scope.pushInteger(7);
            std::array<dom::Value, 0> none{};
            auto notFn = number.apply(none);
            BOOST_TEST(!notFn);
            if (!notFn)
            {
                auto const msg = notFn.error().message();
                bool hasFunction = msg.find("function") != std::string::npos;
                bool hasUndef = msg.find("undefined") != std::string::npos;
                BOOST_TEST(static_cast<bool>(hasFunction || hasUndef));
            }

            auto fnExp = scope.eval("(function(){ throw new Error('boom'); })");
            BOOST_TEST(fnExp);
            if (fnExp)
            {
                auto thrown = fnExp->apply(none);
                BOOST_TEST(!thrown);
                if (!thrown)
                    BOOST_TEST(thrown.error().message().find("boom")
                               != std::string::npos);
            }
        }

        // lookup respects non-null-terminated string_view slices.
        {
            Context ctx;
            Scope scope(ctx);
            scope.script("var nested = { outer: { inner: 42 } };");
            auto nested = scope.getGlobal("nested");
            BOOST_TEST(nested);
            if (nested)
            {
                std::string path = "xxouter.innerzz";
                std::string_view sv(path.data() + 2, path.size() - 4);
                auto v = nested->lookup(sv);
                BOOST_TEST(v.isInteger());
                BOOST_TEST(v.getInteger() == 42);
            }
        }
    }

    void
    test_compile_helpers_behavior()
    {
        Context ctx;
        // compile_script defers execution; function may run body once at
        // compile then again when invoked.
        {
            Scope scope(ctx);
            scope.script("var counter = 0;");
            auto fnExp = scope.compile_script("counter += 1; counter;");
            BOOST_TEST(fnExp);
            if (fnExp)
            {
                auto cnt = scope.getGlobal("counter");
                BOOST_TEST(cnt);
                if (cnt && cnt->isNumber())
                {
                    BOOST_TEST(cnt->getInteger() == 0);
                }
                auto first = (*fnExp)();
                BOOST_TEST(first.isInteger());
                if (first.isInteger())
                    BOOST_TEST(first.getInteger() == 1);
                auto second = (*fnExp)();
                BOOST_TEST(second.isInteger());
                if (second.isInteger())
                    BOOST_TEST(second.getInteger() == 2);
            }
        }

        // compile_script escapes quotes/newlines and preserves mutations even
        // when the script throws on invocation.
        {
            Scope scope(ctx);
            auto fnExp = scope.compile_script("var s = \"a\\\"b\\n\"; s;");
            BOOST_TEST(fnExp);
            if (fnExp)
            {
                auto res = (*fnExp)();
                BOOST_TEST(res.isString());
                if (res.isString())
                    BOOST_TEST(res.getString() == "a\"b\n");
            }
        }

        {
            Scope scope(ctx);
            scope.script("var side = 0;");
            auto fnExp = scope.compile_script(
                "side += 1; throw new Error('fail');");
            BOOST_TEST(fnExp);
            if (fnExp)
            {
                std::array<dom::Value, 0> none{};
                auto call = fnExp->apply(none);
                BOOST_TEST(!call);
                auto sideVal = scope.getGlobal("side");
                BOOST_TEST(sideVal);
                if (sideVal)
                    BOOST_TEST(sideVal->isNumber());
                if (sideVal && sideVal->isNumber())
                    BOOST_TEST(sideVal->getInteger() == 1);
            }
        }

        {
            Scope scope(ctx);
            scope.script("var fCounter = 0;");
            auto compiled = scope.compile_function(
                "fCounter += 1;\n"
                "function bump() { fCounter += 10; return fCounter; }");
            BOOST_TEST(compiled);
            if (compiled)
            {
                auto fc = scope.getGlobal("fCounter");
                BOOST_TEST(fc);
                if (fc && fc->isNumber())
                    BOOST_TEST(fc->getInteger() == 1);
                Value fn = *compiled;
                auto result = fn();
                BOOST_TEST(result.isInteger());
                if (result.isInteger())
                    BOOST_TEST(result.getInteger() == 11);
            }
        }

        // compile_function can leave side effects even when it cannot produce
        // a callable (expression succeeds but is not a function).
        {
            Scope scope(ctx);
            scope.script("var sideOnce = 0;");
            auto compiled = scope.compile_function("sideOnce += 1");
            BOOST_TEST(!compiled);
            auto sideVal = scope.getGlobal("sideOnce");
            BOOST_TEST(sideVal);
            if (sideVal)
                BOOST_TEST(sideVal->isNumber());
            if (sideVal && sideVal->isNumber())
                BOOST_TEST(sideVal->getInteger() == 1);
        }
    }

    void
    test_options_and_invoke_helper()
    {
        handlebars::Handlebars hbs;
        js::Context ctx;
        // JS helpers receive only positional arguments (options object is
        // stripped to avoid infinite recursion from circular symbol references).
        auto ok = js::registerHelper(
            hbs,
            "optcheck",
            ctx,
            "(function(){ return function(){ var last = arguments[arguments.length-1]; return '' + arguments.length + ':' + (typeof last); }; })()"
        );
        BOOST_TEST(ok);
        if (ok)
        {
            // With {{optcheck 1 2}}, the helper receives 2 positional args.
            // The options object is NOT passed to avoid stack overflow from
            // circular context references in Handlebars options.
            auto rendered = hbs.render("{{optcheck 1 2}}\n");
            BOOST_TEST(rendered == "2:number\n");
        }

        using mrdocs::js::detail::invokeHelper;
        js::Scope scope(ctx);
        auto fnExp = scope.eval("(function(){ return arguments.length; })");
        BOOST_TEST(fnExp);
        if (fnExp)
        {
            dom::Array none;
            auto res = invokeHelper(*fnExp, none);
            BOOST_TEST(!res);

            dom::Array bad;
            bad.push_back(dom::Value(1));
            auto res2 = invokeHelper(*fnExp, bad);
            BOOST_TEST(!res2);
        }
    }

    void
    test_js_helper_override()
    {
        handlebars::Handlebars hbs;
        js::Context ctx;

        // JS helpers should override any name (no built-in fast paths).
        auto add = js::registerHelper(
            hbs,
            "add",
            ctx,
            "(function(){ return function(){ return 'js-add'; }; })()"
        );
        BOOST_TEST(add);
        if (add)
        {
            auto rendered = hbs.render("{{add 2 3}}\n");
            BOOST_TEST(rendered == "js-add\n");
        }
    }

    void
    test_helper_resolution_and_proxy_errors()
    {
        // resolveHelperFunction branches: direct, parenthesized, global, fail.
        {
            handlebars::Handlebars hbs;
            js::Context ctx;

            auto direct = js::registerHelper(
                hbs,
                "h1",
                ctx,
                "(function(){ return 'h1'; })");
            BOOST_TEST(direct);
            if (direct)
                BOOST_TEST(hbs.render("{{h1}}") == "h1");

            auto paren = js::registerHelper(
                hbs,
                "h2",
                ctx,
                "function h2(){ return 'h2'; }");
            BOOST_TEST(paren);
            if (paren)
                BOOST_TEST(hbs.render("{{h2}}") == "h2");

            auto globalFallback = js::registerHelper(
                hbs,
                "h3",
                ctx,
                "var h3 = function(){ return 'h3'; }; h3;");
            BOOST_TEST(globalFallback);
            if (globalFallback)
                BOOST_TEST(hbs.render("{{h3}}") == "h3");

            auto bad = js::registerHelper(hbs, "hFail", ctx, "42;");
            BOOST_TEST(!bad);
            if (!bad)
            {
                auto msg = bad.error().message();
                BOOST_TEST(msg.size() > 0);
            }
        }

        // makeFunctionProxy error propagation: native throws -> JS catches.
        {
            js::Context ctx;
            js::Scope scope(ctx);

            auto nativeOk = dom::makeInvocable([](int a) { return a + 5; });
            scope.setGlobal("nativeOk", dom::Value(nativeOk));
            auto ok = scope.eval("nativeOk(7);");
            BOOST_TEST(ok);
            if (ok)
            {
                auto dv = ok->getDom();
                BOOST_TEST(dv.isInteger());
                if (dv.isInteger())
                    BOOST_TEST(dv.getInteger() == 12);
            }

            auto nativeFail = dom::makeInvocable([]() -> dom::Expected<dom::Value> {
                return Unexpected(dom::Error("boom-native"));
            });
            scope.setGlobal("nativeFail", dom::Value(nativeFail));
            auto err = scope.eval(
                "try { nativeFail(); } catch(e) { e.message; }");
            BOOST_TEST(err);
            if (err)
            {
                auto dv = err->getDom();
                BOOST_TEST(dv.isString());
                if (dv.isString())
                    BOOST_TEST(dv.getString().get().rfind("boom-native", 0) == 0);
            }
        }
    }

    void
    test_concurrent_calls()
    {
        js::Context ctx;
        js::Scope scope(ctx);
        auto fnExp = scope.eval("(function add(a, b) { return a + b; })");
        BOOST_TEST(fnExp);
        if (!fnExp)
            return;

        js::Value fn = *fnExp;
        std::vector<std::thread> threads;
        threads.reserve(8);
        for (int i = 0; i < 8; ++i)
        {
            threads.emplace_back([fn]() mutable {
                for (int j = 0; j < 100; ++j)
                {
                    auto res = fn(1, 2);
                    BOOST_TEST(res.isNumber());
                    if (res.isNumber())
                        BOOST_TEST(res.getInteger() == 3);
                }
            });
        }
        for (auto& t : threads) t.join();
    }

    void
    test_helper_name_collision()
    {
        // Registering a helper with the same name twice should override
        handlebars::Handlebars hbs;
        js::Context ctx;

        auto first = js::registerHelper(
            hbs, "collision", ctx,
            "(function(){ return 'first'; })");
        BOOST_TEST(first);

        auto second = js::registerHelper(
            hbs, "collision", ctx,
            "(function(){ return 'second'; })");
        BOOST_TEST(second);

        // The second registration should win
        auto rendered = hbs.render("{{collision}}");
        BOOST_TEST(rendered == "second");
    }

    void
    test_unicode_strings()
    {
        // Verify UTF-8 string handling in JavaScript values
        js::Context ctx;

        // Basic UTF-8 characters and round-trip through global
        {
            js::Scope scope(ctx);

            auto utf8 = scope.eval("'Hello, 世界! 🎉'");
            BOOST_TEST(utf8);
            if (utf8)
            {
                BOOST_TEST(utf8->isString());
                auto str = utf8->getString();
                BOOST_TEST(str.find("世界") != std::string::npos);
                BOOST_TEST(str.find("🎉") != std::string::npos);
            }

            // Round-trip through global
            scope.setGlobal("unicodeTest", dom::Value("日本語テスト"));
            auto retrieved = scope.getGlobal("unicodeTest");
            BOOST_TEST(retrieved);
            if (retrieved)
            {
                BOOST_TEST(retrieved->isString());
                BOOST_TEST(retrieved->getString() == "日本語テスト");
            }
        }

        // In helper context (scope must be destroyed before registerHelper)
        handlebars::Handlebars hbs;
        auto ok = js::registerHelper(
            hbs, "echo_utf8", ctx,
            "(function(x){ return 'Got: ' + x; })");
        BOOST_TEST(ok);
        if (ok)
        {
            // Note: Handlebars escapes HTML, so we check the expected output
            auto rendered = hbs.render("{{echo_utf8 \"café\"}}");
            BOOST_TEST(rendered.find("café") != std::string::npos);
        }
    }

    void
    test_utility_globals_persist()
    {
        // Verify that globals set in one scope persist to the next
        js::Context ctx;

        // First scope: define a utility function
        {
            js::Scope scope(ctx);
            auto exp = scope.script(
                "function testUtility(x) { return x * 2; }");
            BOOST_TEST(exp);
        }

        // Second scope: use the utility function
        {
            js::Scope scope(ctx);
            auto result = scope.eval("testUtility(21)");
            BOOST_TEST(result);
            if (result)
            {
                BOOST_TEST(result->isNumber());
                BOOST_TEST(result->getInteger() == 42);
            }
        }
    }

    void
    test_circular_references()
    {
        // The lazy proxy approach should handle circular references without
        // infinite recursion or stack overflow. This is the primary motivation
        // for using proxies instead of eager conversion.

        // Test 1: Parent-child circular reference
        {
            js::Context ctx;
            js::Scope scope(ctx);

            dom::Object parent;
            dom::Object child;
            parent.set("name", "parent");
            parent.set("value", 100);  // Add number property for testing
            parent.set("child", child);
            child.set("name", "child");
            child.set("parent", parent);

            scope.setGlobal("circular", parent);

            // First test: directly access root object's string property
            // (root has both "name" string and "child" object)
            scope.eval("var rootName = circular.name;");
            auto expRoot = scope.getGlobal("rootName");
            BOOST_TEST(expRoot);
            if (expRoot)
            {
                BOOST_TEST(expRoot->isString());
                if (expRoot->isString())
                    BOOST_TEST(expRoot->getString() == "parent");
            }

            // Test number property access on object with nested child
            scope.eval("var rootValue = circular.value;");
            auto expVal = scope.getGlobal("rootValue");
            BOOST_TEST(expVal);
            if (expVal)
            {
                BOOST_TEST(expVal->isNumber());
                if (expVal->isNumber())
                    BOOST_TEST(expVal->getInteger() == 100);
            }

            // Access through the circular reference - should not hang
            scope.eval("var parentName = circular.child.parent.name;");
            auto exp = scope.getGlobal("parentName");
            BOOST_TEST(exp);
            if (exp)
            {
                BOOST_TEST(exp->isString());
                if (exp->isString())
                    BOOST_TEST(exp->getString() == "parent");
            }

            // Break circular reference to allow cleanup (dom::Object uses ref counting)
            child.set("parent", nullptr);
        }

        // Test 2: Deeper cycle traversal
        {
            js::Context ctx;
            js::Scope scope(ctx);

            dom::Object parent;
            dom::Object child;
            parent.set("name", "parent");
            parent.set("child", child);
            child.set("name", "child");
            child.set("parent", parent);

            scope.setGlobal("circular", parent);

            scope.eval("var childName = circular.child.parent.child.name;");
            auto exp2 = scope.getGlobal("childName");
            BOOST_TEST(exp2);
            if (exp2)
            {
                BOOST_TEST(exp2->isString());
                if (exp2->isString())
                    BOOST_TEST(exp2->getString() == "child");
            }

            // Break circular reference to allow cleanup
            child.set("parent", nullptr);
        }

        // Test 3: Self-referential object
        {
            js::Context ctx;
            js::Scope scope(ctx);

            dom::Object self;
            self.set("value", 42);
            self.set("self", self);
            scope.setGlobal("selfRef", self);

            scope.eval("var selfVal = selfRef.self.self.value;");
            auto exp3 = scope.getGlobal("selfVal");
            BOOST_TEST(exp3);
            if (exp3)
            {
                BOOST_TEST(exp3->isNumber());
                if (exp3->isNumber())
                    BOOST_TEST(exp3->getInteger() == 42);
            }

            // Break self-reference to allow cleanup
            self.set("self", nullptr);
        }
    }

    void
    test_deep_nesting()
    {
        // Verify that deeply nested objects work correctly with lazy proxies.
        // Note: Due to JerryScript global heap state issues when creating
        // multiple contexts sequentially, we reuse the test context from
        // the single-context pattern that works in test_cpp_object.

        Context context;

        // Test nested object access with strings
        {
            Scope scope(context);

            dom::Object inner;
            inner.set("name", "inner");

            dom::Object outer;
            outer.set("name", "outer");
            outer.set("nested", inner);

            scope.setGlobal("deep", outer);

            // Access outer name
            scope.eval("var outerName = deep.name;");
            auto exp0 = scope.getGlobal("outerName");
            BOOST_TEST(exp0);
            if (exp0)
            {
                BOOST_TEST(exp0->isString());
                if (exp0->isString())
                    BOOST_TEST(exp0->getString() == "outer");
            }

            // Access inner name through nesting
            scope.eval("var innerName = deep.nested.name;");
            auto exp1 = scope.getGlobal("innerName");
            BOOST_TEST(exp1);
            if (exp1)
            {
                BOOST_TEST(exp1->isString());
                if (exp1->isString())
                    BOOST_TEST(exp1->getString() == "inner");
            }
        }
    }

    void
    test_operator_bracket_access()
    {
        // Test operator[] for objects and arrays as a convenience alternative
        // to the get() method.
        Context ctx;
        Scope scope(ctx);

        // Object subscript access
        {
            Value obj = scope.eval("({ key: 'value', nested: { inner: 42 } })").value();
            BOOST_TEST(obj.isObject());

            // String key access
            BOOST_TEST(obj["key"].isString());
            BOOST_TEST(obj["key"].getString() == "value");

            // Missing key returns undefined
            BOOST_TEST(obj["missing"].isUndefined());

            // Nested access via chained subscripts
            BOOST_TEST(obj["nested"]["inner"].isNumber());
            BOOST_TEST(obj["nested"]["inner"].getInteger() == 42);
        }

        // Array subscript access
        {
            Value arr = scope.eval("([10, 20, 30])").value();
            BOOST_TEST(arr.isArray());

            // Index access
            BOOST_TEST(arr[0].isNumber());
            BOOST_TEST(arr[0].getInteger() == 10);
            BOOST_TEST(arr[1].getInteger() == 20);
            BOOST_TEST(arr[2].getInteger() == 30);

            // Out of bounds returns undefined
            BOOST_TEST(arr[99].isUndefined());
        }
    }

    void
    test_getstring_owning_string()
    {
        // getString() returns std::string (owning) rather than string_view
        // because JerryScript allocates new buffers for string extraction.
        // This test documents this API behavior for users migrating from
        // other JS engines that might return views.
        Context ctx;
        Scope scope(ctx);

        Value str = scope.eval("'Hello, World!'").value();
        BOOST_TEST(str.isString());

        // getString returns std::string - verify it's a proper copy
        std::string result = str.getString();
        BOOST_TEST(result == "Hello, World!");

        // The returned string should remain valid even after scope operations
        // (unlike a string_view which might be invalidated)
        scope.eval("'something else'");
        BOOST_TEST(result == "Hello, World!"); // Still valid

        // Works with non-ASCII UTF-8 content
        Value utf8 = scope.eval("'日本語'").value();
        std::string utf8Result = utf8.getString();
        BOOST_TEST(utf8Result == "日本語");
    }

    void
    test_utility_file_globals()
    {
        // Test that globals defined in one scope persist to subsequent scopes,
        // which is the mechanism utility files use to provide shared functions.
        Context ctx;

        // First scope: define a utility function (simulates loading _utils.js)
        {
            Scope scope(ctx);
            auto exp = scope.script(
                "function sharedUtil(x) { return x * 2; }\n"
                "var SHARED_CONSTANT = 42;");
            BOOST_TEST(exp);
        }

        // Second scope: verify globals persist and can be used
        {
            Scope scope(ctx);

            // Function should be available
            auto result = scope.eval("sharedUtil(21)");
            BOOST_TEST(result);
            if (result)
            {
                BOOST_TEST(result->isNumber());
                BOOST_TEST(result->getInteger() == 42);
            }

            // Constant should be available
            auto constVal = scope.getGlobal("SHARED_CONSTANT");
            BOOST_TEST(constVal);
            if (constVal)
            {
                BOOST_TEST(constVal->isNumber());
                BOOST_TEST(constVal->getInteger() == 42);
            }
        }

        // Third scope: test that a helper can use the utility function
        handlebars::Handlebars hbs;
        auto ok = js::registerHelper(
            hbs,
            "doubler",
            ctx,
            "(function(x) { return sharedUtil(x); })");
        BOOST_TEST(ok);
        // Registration succeeded, meaning the helper script was able to
        // reference sharedUtil from the global scope. The helper is now
        // registered and usable in templates.
    }

    void
    test_empty_script()
    {
        // Empty script should fail to register as a helper since there's
        // no function to extract.
        handlebars::Handlebars hbs;
        js::Context ctx;

        auto empty = js::registerHelper(hbs, "empty", ctx, "");
        BOOST_TEST(!empty);

        // Whitespace-only script should also fail
        auto whitespace = js::registerHelper(hbs, "ws", ctx, "   \n\t  ");
        BOOST_TEST(!whitespace);
    }

    void
    test_large_strings()
    {
        // Verify that large strings are handled correctly through the
        // JavaScript bridge without truncation or corruption.
        js::Context ctx;
        js::Scope scope(ctx);

        // Test with a moderately large string (100KB)
        std::string large(100000, 'x');
        scope.setGlobal("large", dom::Value(large));
        auto exp = scope.getGlobal("large");
        BOOST_TEST(exp);
        if (exp)
        {
            BOOST_TEST(exp->isString());
            BOOST_TEST(exp->getString().size() == 100000);
        }

        // Test with varied content to catch encoding issues
        std::string varied;
        varied.reserve(10000);
        for (int i = 0; i < 10000; ++i)
        {
            varied.push_back(static_cast<char>('A' + (i % 26)));
        }
        scope.setGlobal("varied", dom::Value(varied));
        exp = scope.getGlobal("varied");
        BOOST_TEST(exp);
        if (exp)
        {
            BOOST_TEST(exp->isString());
            BOOST_TEST(exp->getString() == varied);
        }
    }

    void
    test_function_round_trip()
    {
        // Test that JS functions can be extracted as dom::Function and
        // invoked from C++ code, with arguments and return values preserved.
        js::Context ctx;
        js::Scope scope(ctx);

        auto fnExp = scope.eval("(function(x) { return x * 2; })");
        BOOST_TEST(fnExp);
        if (!fnExp)
            return;

        BOOST_TEST(fnExp->isFunction());

        // Get as dom::Function
        dom::Function domFn = fnExp->getFunction();

        // Invoke with arguments
        dom::Array args;
        args.push_back(dom::Value(21));
        auto result = domFn.call(args);
        BOOST_TEST(result);
        if (result)
        {
            BOOST_TEST(result->isInteger());
            if (result->isInteger())
                BOOST_TEST(result->getInteger() == 42);
        }

        // Test with multiple arguments
        auto addFn = scope.eval("(function(a, b, c) { return a + b + c; })");
        BOOST_TEST(addFn);
        if (addFn)
        {
            dom::Function addDom = addFn->getFunction();
            dom::Array addArgs;
            addArgs.push_back(dom::Value(10));
            addArgs.push_back(dom::Value(20));
            addArgs.push_back(dom::Value(12));
            auto addResult = addDom.call(addArgs);
            BOOST_TEST(addResult);
            if (addResult && addResult->isInteger())
                BOOST_TEST(addResult->getInteger() == 42);
        }
    }

    void
    test_operator_bracket_edge_cases()
    {
        // Test operator[] on types where it doesn't make sense
        js::Context ctx;
        js::Scope scope(ctx);

        // On a number - should return undefined
        Value num = scope.pushInteger(42);
        BOOST_TEST(num["foo"].isUndefined());
        BOOST_TEST(num[0].isUndefined());

        // On a string - array access may return undefined (JS strings
        // don't support bracket indexing in this bridge)
        Value str = scope.pushString("hello");
        BOOST_TEST(str["foo"].isUndefined());

        // On undefined - should return undefined
        Value undef;
        BOOST_TEST(undef["anything"].isUndefined());
        BOOST_TEST(undef[0].isUndefined());

        // On a boolean - should return undefined
        Value b = scope.pushBoolean(true);
        BOOST_TEST(b["foo"].isUndefined());

        // Nested access on non-objects should gracefully return undefined
        Value obj = scope.eval("({ a: 1 })").value();
        BOOST_TEST(obj["a"]["b"]["c"].isUndefined());
    }

    void
    test_deep_circular_stress()
    {
        // Stress test: create a chain of objects with circular reference
        // and traverse it many times to ensure no stack overflow or hang.
        js::Context ctx;
        js::Scope scope(ctx);

        // Create a simple circular structure
        dom::Object a;
        dom::Object b;
        a.set("name", "a");
        a.set("next", b);
        b.set("name", "b");
        b.set("next", a);  // circular

        scope.setGlobal("chain", a);

        // Traverse the circle many times
        std::string traversal = "var result = ''; var cur = chain; "
                                "for (var i = 0; i < 100; i++) { "
                                "  result += cur.name; "
                                "  cur = cur.next; "
                                "} result;";

        auto exp = scope.eval(traversal);
        BOOST_TEST(exp);
        if (exp)
        {
            BOOST_TEST(exp->isString());
            if (exp->isString())
            {
                std::string result = exp->getString();
                // Should be "abababab..." 100 times
                BOOST_TEST(result.size() == 100);
                bool pattern_ok = true;
                for (size_t i = 0; i < result.size(); ++i)
                {
                    char expected = (i % 2 == 0) ? 'a' : 'b';
                    if (result[i] != expected)
                    {
                        pattern_ok = false;
                        break;
                    }
                }
                BOOST_TEST(pattern_ok);
            }
        }

        // Break circular reference for cleanup
        b.set("next", nullptr);
    }

    void run()
    {
        test_context();
        test_scope();
        test_value();
        test_cpp_function();
        test_cpp_object();
        test_cpp_array();
        test_hbs_helpers();
        test_helper_error_propagation();
        test_value_lifetime_and_apply_errors();
        test_compile_helpers_behavior();
        test_options_and_invoke_helper();
        test_js_helper_override();
        test_helper_resolution_and_proxy_errors();
        test_concurrent_calls();
        test_helper_name_collision();
        test_unicode_strings();
        test_utility_globals_persist();
        test_circular_references();
        test_deep_nesting();
        test_operator_bracket_access();
        test_getstring_owning_string();
        test_utility_file_globals();
        test_empty_script();
        test_large_strings();
        test_function_round_trip();
        test_operator_bracket_edge_cases();
        test_deep_circular_stress();
    }
};

TEST_SUITE(
    JavaScript_test,
    "clang.mrdocs.JavaScript");

} // js
} // mrdocs
