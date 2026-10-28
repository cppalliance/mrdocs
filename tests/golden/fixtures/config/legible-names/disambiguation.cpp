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

// A member whose name Windows reserves for a device, ignoring case, adds
// the word of its kind, since a multipage output names a file and a
// directory after its anchor.
namespace aux
{
void reset();
}

struct Nul {};

void com1();
