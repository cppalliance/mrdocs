//
// Copyright (c) 2023 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include <test_suite/detail/decomposer.hpp>
#include <test_suite/diff.hpp>
#include <test_suite/test_suite.hpp>
#include "lib/AST/ParseRef.hpp"

namespace clang::mrdocs {

struct ParseRef_test
{

#define ok(str) BOOST_TEST(parseRef(str).has_value())
#define fail(str) BOOST_TEST(!parseRef(str).has_value())

void
testComponents()
{
    fail("");
    ok("a");
    ok("  a");
    ok("  a  ");
    ok("::a");
    ok("a::b");
    ok("a::b::c");
    ok("a::~b");
    ok("a:: ~ b");
    ok("a::operator+");
    ok("a::operator()");
    ok("a:: operator () ");
    fail("a:: operator ( ) ");
    ok("a::operator bool");
    fail("a::operator bool::c");
    fail("a::operator+::c");
}

void
testFunctionParameters()
{
    ok("f()");
    ok("f  (  ) ");
    ok("f(void)");
    fail("f(void, void)");
    fail("f(int, void)");
    ok("f(...)");
    ok("f(int)");
    ok("f(this T)");
    fail("f(int, this T)");
    ok("f(int, int)");
    fail("f(,)");
}

void
testParameterDeclarationSpecifiers()
{
    // cv specifiers
    {
        ok("f(const int)");
        fail("f(const const int)");
        ok("f(volatile int)");
        fail("f(volatile volatile int)");
        ok("f(const volatile int)");
    }

    // signed/unsigned specifiers
    {
        ok("f(signed int)");
        ok("f(signed char)");
        fail("f(signed signed int)");
        ok("f(unsigned int)");
        fail("f(unsigned unsigned int)");
        fail("f(signed unsigned int)");
        ok("f(signed)");
        ok("f(unsigned)");
        fail("f(signed A)");
        fail("f(unsigned A)");
        fail("f(signed double)");
        fail("f(unsigned double)");
        fail("f(signed auto)");
        fail("f(unsigned auto)");
    }

    // short/long specifiers
    {
        ok("f(short int)");
        fail("f(short short int)");
        ok("f(long int)");
        ok("f(long long int)");
        fail("f(long long long int)");
        fail("f(long short int)");
        ok("f(short)");
        ok("f(long)");
        fail("f(short A)");
        fail("f(long A)");
        fail("f(short double)");
        ok("f(long double)");
        fail("f(short auto)");
        fail("f(long auto)");
    }

    // auto
    {
        ok("f(auto)");
        ok("f(const auto)");
        ok("f(volatile auto)");
        ok("f(const volatile auto)");
        ok("f(auto const)");
        fail("f(auto int)");
        fail("f(auto auto)");
        fail("f(auto decltype(auto))");
    }

    // decltype(auto)
    {
        ok("f(decltype(auto))");
        fail("f(decltype(auto) int)");
        fail("f(decltype(auto) auto)");
    }

    // decltype(expression)
    {
        ok("f(decltype(1))");
        ok("f(decltype(1 + 1))");
        ok("f(decltype((1) + 2 * (3)))");
        fail("f(decltype(1 + 1) int)");
        fail("f(decltype(1 + 1) auto)");
    }

    // elaborated type specifier
    // typename specifier
    {
        ok("f(class A)");
        ok("f(class A::B)");
        ok("f(struct A)");
        ok("f(union A)");
        ok("f(typename A)");
        ok("f(enum A)");
        ok("f(enum A::B)");
        fail("f(class A::B int)");
        fail("f(class A::B auto)");
    }
}

void
testParameterDeclarators()
{
    // unqualified-id
    {
        ok("f(int x)");
        ok("f(A x)");
        ok("f(A (x))");
        ok("f(A ((x)))");
        ok("f(A (  ((x ) )  ) )");
    }

    // ... identifier
    {
        ok("f(auto...)");
        ok("f(Args... args)");
    }

    // * attr (optional) cv (optional) declarator
    {
        ok("f(A* ptr)");
        ok("f(A *ptr)");
        ok("f(A * ptr)");
        ok("f(A* const ptr)");
        ok("f(A* volatile ptr)");
        ok("f(A* const volatile ptr)");
        ok("f(A* const volatile *ptr)");
        ok("f(A* const volatile * ptr)");
        ok("f(A* const volatile * const ptr)");
        ok("f(A* const volatile * const *ptr)");
        ok("f(A* const volatile * const * ptr)");
        // internal declarators
        ok("f(A*ptr)");
        ok("f(A**ptr)");
        ok("f(Args*...ptr)");
        fail("f(A*&ptr)");
        fail("f(A*&&ptr)");
        fail("f(A* C::* ptr)");
        fail("f(A*[] ptr)");
        fail("f(A*() ptr)");
    }

    // nested-name-specifier * attr (optional) cv (optional) declarator
    {
        ok("f(S C::* D)");
        ok("f(S C::D::* E)");
        // invalid internal declarators
        fail("f(S C::** D)");
        fail("f(S C::*& D)");
        fail("f(S C::*&& D)");
    }

    // & attr (optional) declarator
    {
        ok("f(A& x)");
        ok("f(const A& x)");
        ok("f(A const& x)");
        ok("f(A const&... x)");
        // invalid internal declarators
        fail("f(A&* x)");
        fail("f(A&&& x)");
        fail("f(A&[] x)");
        fail("f(A&() x)");
    }

    // && attr (optional) declarator
    {
        ok("f(A&& x)");
        ok("f(const A&& x)");
        ok("f(A const&& x)");
        ok("f(A const&&... x)");
        // invalid internal declarators
        fail("f(A&&* x)");
        fail("f(A&&&& x)");
        fail("f(A&&[] x)");
        fail("f(A&&() x)");
    }

    // noptr-declarator [ expr (optional) ] attr (optional)
    {
        ok("f(A[])");
        ok("f(A x[])");
        ok("f(A x[][])");
        ok("f(A [64])");
        ok("f(A x[64])");
        ok("f(A x[64][64])");
        ok("f(A x[1+2])");
        ok("f(A x[b[2]+c[4]])");
        ok("f(int (*p)[3])");
        ok("f(int (&a)[3])");
        ok("f(int (&a)[3][6])");
        ok("f(int (&&x)[3][6])");
    }

    // noptr-declarator ( parameter-list ) cv (optional) ref  (optional) except (optional) attr  (optional)
    {
        ok("f(A())");
        ok("f(A (A))"); // -> identifier
        ok("f(A (int, A))");
        ok("f(A (int, A)) noexcept");
        ok("f(A ((int, A))) noexcept");
        fail("f(A fn((int, A))) noexcept");
        ok("f(A (fn(int, A))) noexcept");
        ok("f(A (fn(int, A))) noexcept(true)");
        ok("f(A (fn(int, A))) noexcept(2+2)");
        ok("f(A (fn(int, A))) noexcept((2+5)+(3+2))");
        ok("f(A (fn(int, A))) throw()");
        // noptr-declarator is pointer
        ok("f(A (*fn)(int, A))");
        ok("f(A (*)(int, A))");
        ok("f(A (&)(int, A))");
        ok("f(A (&&)(int, A))");
    }
}

void
testMainFunctionQualifiers()
{
    ok("f(int) const");
    ok("f(int) volatile");
    ok("f(int) &");
    ok("f(int) &&");
    ok("f(int) noexcept");
    ok("f(int) noexcept(true)");
    ok("f(int) noexcept(2+2)");
    ok("f(int) noexcept((2+5)+(3+2))");

    fail("f(int) const const");
    ok("f(int) volatile const");
    ok("f(int) const &");
    ok("f(int) const &&");
    ok("f(int) const noexcept");

    ok("f(int) const volatile");
    fail("f(int) volatile volatile");
    ok("f(int) volatile &");
    ok("f(int) volatile &&");
    ok("f(int) volatile noexcept");

    ok("f(int) const &");
    ok("f(int) volatile &");
    ok("f(int) & noexcept");

    ok("f(int) const &&");
    ok("f(int) volatile &&");
    ok("f(int) && noexcept");
}

void
run()
{
    testComponents();
    testFunctionParameters();
    testParameterDeclarationSpecifiers();
    testParameterDeclarators();
    testMainFunctionQualifiers();
}

};

TEST_SUITE(
    ParseRef_test,
    "clang.mrdocs.ParseRef");

} // clang::mrdocs
