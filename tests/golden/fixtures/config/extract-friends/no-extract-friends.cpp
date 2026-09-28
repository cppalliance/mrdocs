namespace std {
template <class T>
class hash;
}

class A {
    friend std::hash<A>;
};

namespace std {
template <>
class hash<A> {
public:
    unsigned long long
    operator()(const A&) const noexcept
    {
        return 0;
    }
};
}
/// A class whose `operator==` is a hidden friend. `extract-friends` is
/// deprecated and ignored: the function is extracted and the class still
/// lists it as a friend.
class B {
    /// Compare two `B` values.
    friend bool operator==(B, B) { return true; }
};
