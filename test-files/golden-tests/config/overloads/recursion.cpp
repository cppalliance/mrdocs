template <typename T>
struct Foo {
    using Type = T;
};

template <typename T, int N>
struct Foo<T[N]> {
    using Type = Foo<T>;
};

template <typename T, typename List>
struct Bar : Bar<T, Foo<List>> {
    void baz(bool) {}
};

template <typename T, typename... Args>
struct Bar<T, Foo<Args...>> {
    void baz(Args...) {}
};
