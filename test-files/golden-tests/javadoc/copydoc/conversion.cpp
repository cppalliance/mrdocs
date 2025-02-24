class string_type;
class string_view_type;

struct A {
    /** Convert A to a string

        @return A string representation of A
     */
    operator string_type() const;

    /** @copydoc operator string_type()
     */
    operator string_view_type() const;

    /** Convert a string to A

        @param other The string to convert
        @return A representation of the string
     */
    A&
    operator=(string_type const& other);

    /// @copydoc operator=(string_type const&)
    A&
    operator=(string_view_type const& other);
};