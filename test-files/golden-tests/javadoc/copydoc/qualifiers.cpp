struct A {
    class iterator;
    class const_iterator;

    /** Returns an iterator to the beginning

        Returns an iterator to the first element
        of the vector.

        @return Iterator to the first element.
     */
    iterator begin();

    /** Return a const iterator to the beginning

        Returns a const iterator to the first element
        of the vector.

        @return Const iterator to the first element.
     */
    const_iterator begin() const;

    /** @copydoc begin() const
     */
    const_iterator cbegin() const;

    /** An lvalue reference to A

        @return A reference to A
     */
    A& ref() &;

    /** An rvalue reference to A

        @return A reference to A
     */
    A&& ref() &&;

    /** An const lvalue reference to A

        @return A reference to A
     */
    A const& ref() const &;

    /** An const rvalue reference to A

        @return A reference to A
     */
    A const&& ref() const &&;

    /** @copydoc ref() &&
     */
    A&& rvalue() &&;

    /** @copydoc ref() const &&
     */
    A&& crvalue() const &&;
};