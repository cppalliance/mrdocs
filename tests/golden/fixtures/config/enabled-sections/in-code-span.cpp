/** Brief of described.

    Mention `\cond` in a code span.

    The `@endcond` command written with an at sign works the same, and
    the paragraph continues on the next line.

    Double backticks, as in ``\cond INTERNAL``, work as well.

    This paragraph follows the mentions.
*/
int described(int x);

/// Brief of after.
int after(int x);

/// Brief of ticked, ending with a code span `\cond`
int ticked(int x);

/// Brief of final.
int finale(int x);
