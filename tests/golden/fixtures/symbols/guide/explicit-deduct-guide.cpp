//Test deduction guides that can be "explicit", "explicit(true)", "explicit(false)" and "explicit(EXPR)"

template<int>
struct X {};

explicit X(bool) -> X<0>;

explicit(true) X(char) -> X<0>;

explicit(false) X(int) -> X<0>;

template<bool B = true>
explicit(B) X(long) -> X<0>;
