/// A 2D vector.
struct vec2
{
    /// Construct from `x` and `y` components.
    vec2(double x, double y);

    vec2(vec2 const&);
    vec2(vec2&&) noexcept;
    ~vec2();

    vec2& operator=(vec2 const&);

    bool operator==(vec2 const&) const;

    /// Returns the Euclidean length of the vector.
    double magnitude() const;

    /// Translate this vector by `delta`.
    vec2 translated(vec2 const& delta) const;
};
