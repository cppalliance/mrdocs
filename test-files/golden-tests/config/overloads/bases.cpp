struct BaseBase1 {
    void foo(bool);
};

struct BaseBase2 {
    void foo(bool);
};

struct Base1 : public BaseBase1 {
    void foo(int);
};

struct Base2 : private BaseBase2 {
    void foo(double);
};

struct User : public Base1, Base2 {
    void foo(int);
};
