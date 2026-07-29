class B1
{
};

class B2
{
};

class A1 final
{
};

template< typename T >
class A2 final
{
};

class A3 final : public B1, public B2
{
};
