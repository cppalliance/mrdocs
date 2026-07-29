struct tag_base {};

/** A default-constructed instance of a same-named tag type (see issue #1238). */
struct flag final : tag_base {} flag;

/** A copy-initialized variable. */
auto flag_copy = flag;

/** A value-initialized variable. */
tag_base braced{};

/** A variable with a literal initializer. */
int literal = 7;
