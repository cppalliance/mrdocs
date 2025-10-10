struct X
{
    /// A base class to test inheritance and shadowing
    class base
    {
    public:
        /// This function should be shadowed by derived classes.
        base& derived_shadowed();
        /// This function should be inherited by derived classes.
        base& base_inherited();
    protected:
        /// This function should be shadowed by derived classes.
        base& do_derived_shadowed();
        /// This function should be inherited by derived classes.
        base& do_base_inherited();
    };

    /// A class that derives from base and excluded_base
    class derived
        : public base
    {
    public:
        /// This function should shadow the base class function.
        derived& derived_shadowed();
    protected:
        /// This function should shadow the base class function.
        derived& do_derived_shadowed();
    };
};
