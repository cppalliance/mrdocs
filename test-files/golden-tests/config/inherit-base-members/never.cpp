/// A base class excluded from the documentation
class excluded_base {
public:
    /// This function should be shadowed by derived classes.
    excluded_base& derived_shadowed();
    /// This function should be shadowed by the base class.
    excluded_base& base_shadowed();
    /// This function should be inherited by derived classes.
    excluded_base& excluded_inherited();
protected:
    /// This function should be shadowed by derived classes.
    excluded_base& do_shadowed();
    /// This function should be shadowed by the base class.
    excluded_base& do_base_shadowed();
    /// This function should be inherited by derived classes.
    excluded_base& do_excluded_inherited();
};

/// A second-order base class to test indirect inheritance
class base_base {
public:
    /// This function should be indirectly inherited by derived classes.
    base_base& base_base_inherited();
public:
    /// This function should be indirectly inherited by derived classes.
    base_base& do_base_base_inherited();
};

/// A base class to test inheritance and shadowing
class base :
    public base_base
{
public:
    /// This function should be shadowed by derived classes.
    base& derived_shadowed();
    /// This function should shadow the excluded_base function.
    base& base_shadowed();
    /// This function should be inherited by derived classes.
    base& base_inherited();
protected:
    /// This function should be shadowed by derived classes.
    base& do_derived_shadowed();
    /// This function should shadow the excluded_base function.
    base& do_base_shadowed();
    /// This function should be inherited by derived classes.
    base& do_base_inherited();
};

/// A class that derives from base and excluded_base
class derived
    : public base
    , public excluded_base
{
public:
    /// This function should shadow the base class function.
    derived& derived_shadowed();
public:
    /// This function should shadow the base class function.
    derived& do_derived_shadowed();
};

/// A class that should inherit functions as protected.
class protected_derived
    : protected base
    , protected excluded_base
{
public:
    /// This function should shadow the base class function.
    protected_derived& derived_shadowed();
public:
    /// This function should shadow the base class function.
    protected_derived& do_derived_shadowed();
};

/// A class that uses private inheritance only
class private_derived
    : private base
    , private excluded_base
{
public:
    /// This function should shadow the base class function.
    private_derived& derived_shadowed();
public:
    /// This function should shadow the base class function.
    private_derived& do_derived_shadowed();
};
