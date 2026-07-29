struct D {};

template <class T, class U = void>
struct C {};

template <>
struct C<int, char> {};

template <>
struct C<int> {};

template <class T, class U>
struct B {};

template <>
struct B<int, char> {};

template <class U>
struct B<int, U> {};

struct A {};

struct Z {
    auto operator<=>(Z const&) const;
    bool operator!=(Z const&) const;
    bool operator==(Z const&) const;
    bool operator!() const;
    operator bool() const;
    void foo() const;
    ~Z();
    Z(int);
    Z();
};

bool operator!=(A const& lhs, A const& rhs);
bool operator==(A const& lhs, A const& rhs);
bool operator!(A const& v);

void h();

template <class T>
char g(T, T, T);

template <>
char g<int>(int, int, int);

char g(char, char, char);

char g(double, char);

char g(double);

char g(int);

char g(int);

void g();

void f();

