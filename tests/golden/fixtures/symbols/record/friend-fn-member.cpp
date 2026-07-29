struct X
{
    X();
    ~X();
    /// Friend member-function
    /// @param i Description of i
    /// @return char*
    char* foo(int i);
};

struct U
{
    friend X::X();
    friend X::~X();
    friend char* X::foo(int);
};
