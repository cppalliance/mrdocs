template<class X, class Y, typename Z>
class V {};

struct A {};
struct B {};
struct C {};

struct S0 : A, B {};
struct S1 : S0, C {};

template<class T>
struct U {};

struct Z : U<S0> {};

struct Q : protected A, virtual B{};
