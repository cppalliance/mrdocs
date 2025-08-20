namespace A
{
    void f(int);
    void f(void);
    void f(char,char);
}

// This should ultimately resolve to the function overloads in namespace A.
using A::f;
