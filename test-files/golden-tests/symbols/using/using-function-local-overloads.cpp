namespace A
{
    /// First overload
    void f(int);
    /// Second overload
    void f(void);
    /// Third overload
    void f(char,char);
}

/// Local overload
void f(double);

// This should ultimately resolve to a new function overloads page that includes
// the symbols in namespace A.
using A::f;

