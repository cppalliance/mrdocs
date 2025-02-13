using T = void(...);

using U = void(int, ...);

void f(...);

void g(int, ...);

template<typename A = void(...), typename B = void(int, ...)>
struct C;
