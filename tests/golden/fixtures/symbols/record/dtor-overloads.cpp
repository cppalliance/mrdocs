template <class T>
struct A
{
    ~A() requires (sizeof(T) > 4);
    ~A() requires (sizeof(T) <= 4);
};
