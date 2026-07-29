class ConstBase {
public:
    int& foo() const;
};

class Base
    : public ConstBase {
public:
    int& foo();
    int& foo() const;
};

class C : public Base {};