struct Z {
    auto operator<=>(Z const&) const;
    bool operator!=(Z const&) const;
    bool operator==(Z const&) const;
    bool operator!() const;
    operator bool() const;
    void foo() const;
    void bar() const;
    ~Z();
    Z(int);
    Z();
};

