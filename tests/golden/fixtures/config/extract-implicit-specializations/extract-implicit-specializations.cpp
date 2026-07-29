template <typename T>
struct B {
    T& value();
};

struct A : public B<int> {};
