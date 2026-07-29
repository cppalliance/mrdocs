namespace detail {

/// CRTP base that lifts `compare()` into `operator==` and `operator!=`.
template <class T>
struct comparable
{
    /// True if `compare()` is `0`.
    bool operator==(T const& other) const noexcept;
    /// True otherwise.
    bool operator!=(T const& other) const noexcept;
};

}

/// A semantic version `(major, minor)`.
struct version : detail::comparable<version>
{
    /// Construct from major and minor numbers.
    version(int major, int minor) noexcept;

    /// Three-way comparison against another version.
    int compare(version const& other) const noexcept;

    /// The major component.
    int major;
    /// The minor component.
    int minor;
};
