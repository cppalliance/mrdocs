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