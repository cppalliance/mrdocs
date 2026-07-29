#pragma clang diagnostic ignored "-Wdeprecated-volatile"

template<const int x>
void f();
template<volatile int x>
void f();
template<const volatile int x>
void f();
template<int x>
void f();

template<int* x>
void g();
template<int* const x>
void g();
template<int x[]>
void g();
template<int x[4]>
void g();

template<int x(bool)>
void h();
template<int x(const bool)>
void h();
template<int (*x)(bool)>
void h();
template<int (*x)(const bool)>
void h();

using T = int;
using U = const int;

template<T>
void i();
template<U>
void i();
template<const T>
void i();
template<const U>
void i();
