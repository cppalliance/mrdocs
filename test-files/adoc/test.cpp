struct T
{
    T() = default;
    T(T const& other) = delete;

    int f();
    void h(int i);

    struct U
    {
        void g();
    };

    void j(int, char x, bool) noexcept;
};
