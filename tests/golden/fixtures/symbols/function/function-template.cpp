template<typename T>
void f0(int x) { }

template<typename T>
void f1(T t) { }

template<typename T = int>
void f2() { }

template<typename T, class U = int>
void f3() { }

template<int I>
void g0(int x) { }

template<int I = 1>
void g1() { }

template<int J, int I = 1>
void g2() { }

void h0(auto x) { }

void h1(auto x, auto) { }

template<typename T = int, int I = 1>
void i() { }

template<template<typename U> typename T>
void j0() { }

template<
    template<typename W> typename X,
    template<typename Y> typename Z>
void j1() { }
