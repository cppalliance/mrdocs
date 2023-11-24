//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Support/JavaScript.hpp>
#include <test_suite/test_suite.hpp>
#include <array>

namespace clang {
namespace mrdocs {
namespace js {

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
                    BOOST_TEST(
                        (key == "a" || key == "b" || key == "c" || key == "d"));
                    BOOST_TEST(
                        (value.isInteger() || value.isBoolean()
                         || value.isString() || value.isNull()));
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
                    BOOST_TEST(
                        (v.isInteger() || v.isBoolean() || v.isString()
                         || v.isNull()));
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

        // setlog()
        {
            Context context;
            Scope scope(context);
            Value x = scope.eval("({})").value();
            BOOST_TEST(x.isObject());
            x.setlog();
            dom::Value y = x.getDom();
            BOOST_TEST(y.isObject());
            BOOST_TEST(y.exists("log"));
            BOOST_TEST(y.get("log").isFunction());
            BOOST_TEST(y.get("log")(1, "hello world").isUndefined());
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
                BOOST_TEST(!f.empty());
                BOOST_TEST(f.size() == 1);
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

        // callProp()
        {
            Context context;
            Scope scope(context);
            Value x = scope.eval("({ f: function(a, b) { return a + b; } })").value();
            BOOST_TEST(x.isObject());
            BOOST_TEST(x.callProp("f", 1, 2).value().getDom() == 3);
            BOOST_TEST(x.get("f")(1, 2).getDom() == 3);
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
            BOOST_TEST(x1 == undef);
            BOOST_TEST(!(x1 < undef));
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
        {
            // Create C++ function
            Scope scope(context);
            dom::Object o1;
            o1.set("a", 1);
            BOOST_TEST(o1.get("a") == 1);

            // Register proxy to C++ object as JS object
            scope.setGlobal("o", o1);

            // Test C++ object usage from JS
            scope.eval("var x = o.a;");
            auto exp = scope.getGlobal("x");
            BOOST_TEST(exp);
            Value x = *exp;
            BOOST_TEST(x.isNumber());
            BOOST_TEST(x.getDom() == 1);

            // JS changes affect C++ object via the Proxy
            // "set"
            scope.eval("o.a = 2;");
            BOOST_TEST(o1.get("a") == 2);
            // "has"
            scope.eval("var y = 'a' in o;");
            auto yexp = scope.getGlobal("y");
            BOOST_TEST(yexp);
            Value y = *yexp;
            BOOST_TEST(y.isBoolean());
            BOOST_TEST(y.getDom() == true);
            // "deleteProperty" is not allowed
            Expected<Value> de = scope.eval("delete o.a;");
            BOOST_TEST(de);
            BOOST_TEST(!de.value());
            BOOST_TEST(o1.get("a") == 2);
            // "ownKeys"
            scope.eval("var z = Object.keys(o);");
            auto zexp = scope.getGlobal("z");
            BOOST_TEST(zexp);
            Value z = *zexp;
            BOOST_TEST(z.isArray());
            // Missing functionality:
            // https://github.com/svaarala/duktape/issues/2153
            // BOOST_TEST(z.size() == 1);
            // BOOST_TEST(z.get(0).isString());
            // BOOST_TEST(z.get(0).getString() == "a");

            // C++ changes affect JS object via the Proxy
            // "set"
            o1.set("a", 3);
            scope.eval("var x = o.a;");
            auto exp2 = scope.getGlobal("x");
            BOOST_TEST(exp2);
            Value x2 = *exp2;
            BOOST_TEST(x2.isNumber());
            BOOST_TEST(x2.getDom() == 3);
            // "has"
            o1.set("b", 4);
            scope.eval("var y = 'b' in o;");
            auto yexp2 = scope.getGlobal("y");
            BOOST_TEST(yexp2);
            Value y2 = *yexp2;
            BOOST_TEST(y2.isBoolean());
            BOOST_TEST(y2.getDom() == true);
            // "ownKeys"
            o1.set("c", 5);
            scope.eval("var z = Object.keys(o);");
            auto zexp2 = scope.getGlobal("z");
            BOOST_TEST(zexp2);
            Value z2 = *zexp2;
            BOOST_TEST(z2.isArray());
            // Missing functionality:
            // https://github.com/svaarala/duktape/issues/2153
            // BOOST_TEST(z2.size() == 3);
            // BOOST_TEST(z2.get(0).isString());
            // BOOST_TEST(z2.get(0).getString() == "a");
            // BOOST_TEST(z2.get(1).isString());
            // BOOST_TEST(z2.get(1).getString() == "b");
            // BOOST_TEST(z2.get(2).isString());
            // BOOST_TEST(z2.get(2).getString() == "c");

            // Get the C++ object as a JS Value
            auto oexp = scope.getGlobal("o");
            BOOST_TEST(oexp);
            Value o2 = *oexp;
            BOOST_TEST(o2.isObject());
            BOOST_TEST(o2.get("a").getDom() == 3);

            // Get the C++ object as a dom::Value
            dom::Value o3 = o2.getDom();
            BOOST_TEST(o3.isObject());
            BOOST_TEST(o3.get("a") == 3);
        }
    }

    void run()
    {
        test_context();
        test_scope();
        test_value();
        test_cpp_function();
        test_cpp_object();
    }
};

TEST_SUITE(
    JavaScript_test,
    "clang.mrdocs.JavaScript");

} // js
} // mrdocs
} // clang

