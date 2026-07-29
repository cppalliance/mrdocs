template <class T>
class Base {
public:
    using V = T;
};

template <class T>
class Derived : private Base<T> {
private:
    using typename Base<T>::V;
};