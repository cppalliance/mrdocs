/// A struct.
struct S
{
    int x;
    double y;
};

/// Pointer to an int member of S.
int S::* mp_int = &S::x;

/// Pointer to a double member of S.
double S::* mp_double = &S::y;
