namespace lib {
/** Greet the user by name. */
void greet(char const* name);
}

// Empty namespace reserved for future expansion. With
// `extract-empty-namespaces: true` it still appears in the docs.
namespace lib::experimental {}
