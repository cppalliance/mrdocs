union A
{
    int x;
    bool y;
};

struct B
{
    union
    {
        int x;
        bool y;
    };

    int z;
};
