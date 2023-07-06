auto f() { struct A { void g(); } a; return a; } struct B : decltype(f()) { };
