template<typename T>
concept C = sizeof(T) == sizeof(int);

template<C T>
void f();

C auto x = 0;
