// Disambiguate constructors

struct X {
    X();
    X(X const& other);
    X(X&& other);
    X(int);
    X(double);
};