namespace left_shift {

class A {
public:
    // member functions cannot be ostream operators

    A&
    operator<<(unsigned int shift);

    A&
    operator<<(char shift);
};

A
operator<<(const A& obj, double shift);

}

namespace ostream {

class OStream {};

class B {
public:
    friend
    OStream&
    operator<<(OStream&, B);
};

class C {};

OStream&
operator<<(OStream&, C);
}


