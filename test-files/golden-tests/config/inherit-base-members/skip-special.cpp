/*
    A version similar to copy.cpp, with special member functions
    that should not be inherited.
 */

class excluded_base {
public:
    /// Constructor should not be inherited
    excluded_base();

    /// Destructor should not be inherited
    ~excluded_base();

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

class base_base {
public:
    /// Constructor should not be inherited
    base_base();

    /// Destructor should not be inherited
    ~base_base();

    /// This function should be indirectly inherited by derived classes.
    base_base& base_base_inherited();
protected:
    /// This function should be indirectly inherited by derived classes.
    base_base& do_base_base_inherited();
};

class base :
    public base_base
{
public:
    /// Constructor should not be inherited
    base();

    /// Destructor should not be inherited
    ~base();

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

class derived
    : public base
    , public excluded_base
{
public:
    /// Constructor should not be inherited
    derived();

    /// Destructor should not be inherited
    ~derived();

    /// This function should shadow the base class function.
    derived& derived_shadowed();
protected:
    /// This function should shadow the base class function.
    derived& do_derived_shadowed();
};

/// Should inherit functions as protected.
class protected_derived
    : protected base
    , protected excluded_base
{
public:
    /// Constructor should not be inherited
    protected_derived();

    /// Destructor should not be inherited
    ~protected_derived();

    /// This function should shadow the base class function.
    protected_derived& derived_shadowed();
protected:
    /// This function should shadow the base class function.
    protected_derived& do_derived_shadowed();
};

class private_derived
    : private base
    , private excluded_base
{
public:
    /// Constructor should not be inherited
    private_derived();

    /// Destructor should not be inherited
    ~private_derived();

    /// This function should shadow the base class function.
    private_derived& derived_shadowed();
protected:
    /// This function should shadow the base class function.
    private_derived& do_derived_shadowed();
};
