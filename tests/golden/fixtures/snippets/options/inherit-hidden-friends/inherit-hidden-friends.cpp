namespace detail {

/// CRTP base that lifts `compare()` into a hidden-friend `operator==`.
template <class T>
struct equality_comparable
{
    /// True if `a.compare(b)` is `0`.
    friend bool operator==(T const& a, T const& b) noexcept;
};

}

/// A temperature in degrees Celsius.
struct temperature : detail::equality_comparable<temperature>
{
    /// Construct from a value in degrees Celsius.
    temperature(double celsius) noexcept;

    /// Three-way comparison against another temperature.
    int compare(temperature const& other) const noexcept;

    /// The value in degrees Celsius.
    double celsius;
};
