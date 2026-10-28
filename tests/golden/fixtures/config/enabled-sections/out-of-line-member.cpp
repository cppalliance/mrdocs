/// Visible record.
struct S
{
    /// Visible member.
    void visible();

    /// \cond INTERNAL
    /// Hidden member.
    void secret();
    /// \endcond
};

/// Out-of-line definition of the visible member.
inline void S::visible()
{
}

/// Out-of-line definition of the hidden member.
inline void S::secret()
{
}
