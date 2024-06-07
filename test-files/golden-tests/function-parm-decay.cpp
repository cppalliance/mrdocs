#pragma clang diagnostic ignored "-Wdeprecated-volatile"

void f(const int x);
void f(volatile int x);
void f(const volatile int x);
void f(int x);

void g(int* x);
void g(int* const x);
void g(int x[]);
void g(int x[4]);

void h(int x(bool));
void h(int x(const bool));
void h(int (*x)(bool));
void h(int (*x)(const bool));

using T = int;
using U = const int;

void i(T);
void i(U);
void i(const T);
void i(const U);
