class widget
{
    /** A tag the caller can hold but not name.

        @implementationdefined
    */
    struct token {};

public:
    /** Acquire the widget.

        @return A token that releases the widget when destroyed.
    */
    token acquire();
};
