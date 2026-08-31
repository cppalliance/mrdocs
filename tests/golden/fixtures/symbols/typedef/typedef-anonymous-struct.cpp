// A `typedef struct { ... } Name;` gives its name for linkage to an otherwise
// anonymous record. MrDocs extracts it as a single record named `Name` rather
// than a nameless record plus a redundant `Name` alias, since the anonymous
// struct has no other name a user could refer to. A typedef of a named type is
// still extracted as an alias.

/// A configuration value.
typedef struct {
    /// The section name.
    char* section;
    /// The entry name.
    char* name;
    /// The entry value.
    char* value;
} CONF_VALUE;

/// An alias for a named type stays a typedef.
typedef int handle_t;
