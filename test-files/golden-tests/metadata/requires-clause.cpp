template<typename T>
void f() requires (sizeof(T) == 4);

template<typename T>
void f() requires (sizeof(T) == 2);

template<typename U>
void f() requires (sizeof(U) == 2);

template<typename T> requires (sizeof(T) == 4)
void g();

template<typename T> requires (sizeof(T) == 2)
void g();

template<typename U> requires (sizeof(U) == 2)
void g();

template<typename T> requires (sizeof(T) == 2)
struct A;

template<typename U> requires (sizeof(U) == 2)
struct A;
