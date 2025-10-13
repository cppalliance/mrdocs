struct Base {
void foo();
void bar();
void baz();
};

struct Derived : Base {
    void foo();
protected:
    void bar();
private:
    void baz();
};
