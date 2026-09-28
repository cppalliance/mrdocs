namespace lib {

/// Base with hidden friends, documented on its own page.
struct base
{
    /// Compares two `base` objects.
    friend bool operator==(base, base) { return true; }

    /// Checks whether the left operand orders first.
    friend bool less(base, base) { return true; }
};

/// Derives from the documented base; its hidden friends are inherited.
struct wrapped : base
{
};

namespace detail {

/// Base excluded from the documentation through `exclude-symbols`.
struct hidden_base
{
    /// Compares two `hidden_base` objects.
    friend bool operator==(hidden_base, hidden_base)
    {
        return true;
    }
};

}

/// Derives from the excluded base; its hidden friend is inherited.
struct value : detail::hidden_base
{
};

}
