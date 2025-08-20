namespace A
{
    void f(int);
    void f(void);
    void f(char,char);
}

void f(double i);

// This should ultimately resolve to a new function overloads page that includes
// the symbols in namespace A.
using A::f;

