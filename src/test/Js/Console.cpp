//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <lib/Js/Console.hpp>
#include <lib/Js/StdGlobals.hpp>

#include <mrdocs/Support/JavaScript.hpp>
#include <test_suite/test_suite.hpp>

namespace mrdocs {
namespace js {

struct Console_test
{
    void
    test_register_console_installs_global()
    {
        Context ctx;
        Scope scope(ctx);
        registerConsole(scope);

        auto consoleVal = scope.getGlobal("console");
        if (!BOOST_TEST(consoleVal.has_value()))
        {
            return;
        }
        BOOST_TEST(consoleVal->isObject());

        // log and error must be callable from script.
        BOOST_TEST(scope.eval("typeof console.log").value().getString() == "function");
        BOOST_TEST(scope.eval("typeof console.error").value().getString() == "function");

        // Invocation must not raise; the call returns undefined.
        BOOST_TEST(scope.eval("console.log('hi'); 1").value().getDom() == 1);
        BOOST_TEST(scope.eval("console.error('boom'); 2").value().getDom() == 2);

        // Objects render as JSON; the round-trip script reads the
        // length of the JSON string back without touching stdout.
        Expected<Value> stringified =
            scope.eval("JSON.stringify({a: 1}).length");
        if (!BOOST_TEST(stringified.has_value()))
        {
            return;
        }
        BOOST_TEST(stringified->getDom() == 7);
    }

    void
    test_register_stdglobals_installs_console()
    {
        Context ctx;
        Scope scope(ctx);
        registerStdGlobals(scope);

        // StdGlobals today is just the console; verify it landed.
        BOOST_TEST(scope.eval("typeof console.log").value().getString() == "function");
    }

    void
    run()
    {
        test_register_console_installs_global();
        test_register_stdglobals_installs_console();
    }
};

TEST_SUITE(
    Console_test,
    "clang.mrdocs.Js.Console");

} // js
} // mrdocs
