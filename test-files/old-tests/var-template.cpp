template<typename T>
unsigned A = sizeof(T);

template<>
unsigned A<void> = 0;

template<typename T>
unsigned A<T&> = sizeof(T);

struct B
{
    template<typename T>
    static inline unsigned C = 0;

    template<>
    static inline unsigned C<int> = -1;

    template<typename T>
    static inline unsigned C<T*> = sizeof(T);
};