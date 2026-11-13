// Members that share a name get anchors that tell them apart by their
// kind and by their signature, not by their symbol IDs.

// The overload set keeps the plain name, and each function adds the
// number of its parameters, then their types and its qualifiers, as far
// as it needs to.
struct Widget
{
    void resize();
    void resize(int n);
    void resize(int n, int m);
    void resize(double n, int m);

    int& at(int i);
    int const& at(int i) const;

    // A top-level `const` on a parameter is not part of the function's
    // type, so it is not spelled, and does not make the first overload
    // spell like the second.
    void reset(int* const p);
    void reset(int* p) const;

    // A conversion function is named after the type it converts to, and
    // conversions to one type form an overload set like other functions.
    operator int() const;
    operator int();
    operator double() const;
    operator bool();
};

// Of the overload sets of one name that a class splits by access, the
// public one keeps the plain name, and the protected one adds its access.
struct Access
{
    void set();
    void set(int n);

protected:
    void set(double d);
    void set(char c);
};

// Access comes first: a public function with no public overloads keeps
// the plain name next to a protected overload set, which adds its access.
struct Exposed
{
    void get(int n);

protected:
    void get(double d);
    void get(char c);
};

// Of two functions of one name that a class splits by access, neither
// with an overload in its access, the public one keeps the plain name,
// and the protected one adds the number of its parameters.
struct Guarded
{
    void foo(int n);

protected:
    void foo(double d);
};

// Of two functions of one name with the same access, one static and one
// not, neither with an overload, the non-static one keeps the plain name.
struct Shared
{
    void bar(int n);

    static void bar(double d);
};

struct a_parameter_type_whose_name_is_too_long_to_spell_in_an_anchor {};

template <class T>
struct Wrapper {};

// An overload whose spelled signature is too long for an anchor adds a
// hash of the signature in its place, and a conversion function whose
// target type is too long to spell adds a hash of the type after
// `2conversion`.
struct Settings
{
    void apply(int n);
    void apply(
        a_parameter_type_whose_name_is_too_long_to_spell_in_an_anchor const& p);
    void apply(
        a_parameter_type_whose_name_is_too_long_to_spell_in_an_anchor&& p);

    operator Wrapper<a_parameter_type_whose_name_is_too_long_to_spell_in_an_anchor>();
};

// The primary template keeps the plain name, and each specialization
// adds the number of its template arguments, then the arguments.
template <class T, class U>
struct Pair {};

template <>
struct Pair<int, int> {};

template <class T>
struct Pair<T*, T> {};

template <class T>
struct Box {};

template <>
struct Box<char> {};

// Parameters, like template arguments, are separated by `comma`, so that
// two lists whose types differ only in where one of them ends do not
// spell alike.
struct Boundaries
{
    void pick(long a, long long b);
    void pick(long long a, long b);
};

template <>
struct Pair<long, long long> {};

template <>
struct Pair<long long, long> {};

// A specialization of a function template adds its template arguments
// when nothing else tells it apart.
template <class T>
void make();

template <>
void make<int>();

template <>
void make<char>();

// Every punctuation character is spelled, so a negative argument does not
// spell like a positive one, character arguments do not spell alike, and
// template arguments keep their nesting.
template <int N>
struct Offset {};

template <>
struct Offset<-1> {};

template <>
struct Offset<1> {};

template <char C>
struct Key {};

template <>
struct Key<'$'> {};

template <>
struct Key<'@'> {};

template <class... T>
struct Many {};

struct Punctuated
{
    void nest(Many<Many<int>, char>);
    void nest(Many<Many<int, char>>);
};

// An identifier outside ASCII is spelled whole.
struct Größe {};

struct Grüße {};

struct Sized
{
    void scale(Größe);
    void scale(Grüße);
};

// A parameter declared as an array or as a function is spelled as the
// pointer it is adjusted to, which is its type in the function's type,
// also when it is a parameter of a function type a parameter is built
// from.
struct Adjusted
{
    void fill(int a[3]);
    void fill(char* p);

    void call(void f(int));
    void call(int n);

    void pass(void (*f)(int a[3]));
    void pass(int n);
};

// Between kinds, the class keeps the plain name: a concept whose name
// differs only in case, a function and deduction guides add the word of
// their kind.
struct quantity {};

template <class T>
concept Quantity = true;

struct stat {};

int stat(char const* path, struct stat* buf);

template <class T>
struct Holder
{
    Holder(T);
};

Holder(char const*) -> Holder<char const*>;

Holder(int) -> Holder<long>;

// The global namespace keeps the name `index`, so a struct whose name
// differs from it only in case adds `class`, the word of every record.
struct Index {};

// A namespace keeps the plain name, and a namespace alias whose name
// differs from it only in case adds `2namespace-alias`.
namespace Region
{
struct Area {};
}

namespace region = Region;

// A class keeps the plain name; an alias-declaration whose name differs
// only in case from the class, and a using-declaration of the class's
// own name, add different words.
namespace N
{
void X();
}

struct X {};

using x = int;

using N::X;
