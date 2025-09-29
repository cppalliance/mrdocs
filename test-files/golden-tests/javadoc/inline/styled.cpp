/** Brief for A

    Paragraph with `code`, **bold** text, and _italic_ text.

    We can also escape these markers: \`, \*, and \_.

 */
struct A {
    /** Compare function

        @param other The other object to compare against.

	@return `-1` if `*this < other`, `0` if
        `this == other`, and 1 if `this > other`.
     */
    int compare(A const& other) const noexcept;
};

