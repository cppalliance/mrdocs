struct A {
    /** Return true if ch is in the character set.

        This function returns true if the
        character is in the set, otherwise
        it returns false.

        @param ch The unsigned character to test.
    */
    constexpr
    bool
    operator()(unsigned char ch) const noexcept;

    /**
        @copydoc A::operator()(unsigned char) const

        @param ch The signed character to test.
     */
    constexpr
    bool
    operator()(char ch) const noexcept;
};