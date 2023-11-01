/// Struct T brief
struct T
{
    /// Friend int brief
    friend int;

    /// Friend class Z brief
    friend class Z;
};

/// Struct U brief
struct U
{
    /// Friend T brief
    friend T;
};

/// Struct V brief
struct V
{
    /// Friend struct U brief
    friend struct U;
};
