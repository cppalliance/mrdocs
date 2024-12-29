/*
    The test explores all combinations of filters and extraction modes:

    * Global namespace that includes symbols with different filters
    * Namespaces that pass each of the filters
    * Each namespace has a symbol that passes each of the filters

 */

/// A regular namespace with different filters for members
namespace regular_ns
{
    /// A symbol that passes the filters
    ///
    /// The symbol should have a page as usual
    struct regular {
        /// Child of a regular symbol extracted as regular
        struct also_regular {
            /// Grandchild of a regular symbol extracted as regular
            struct regular_as_well {};
        };
    };

    /// A function to get a regular symbol
    ///
    /// When used in a function, the symbol should be shown as usual
    /// with a link to the page.
    regular get_regular() { return {}; }

    /// A symbol that passes the see-below filter
    ///
    /// A symbol that passes the filters and the see-below filter.
    /// The symbol should have a page as usual but, because it's a scope
    /// and not a namespace, the members should not be listed on that page.
    struct see_below {
        /// Child of a see-below struct: should not be traversed
        struct also_see_below {
            /// Grandchild of a see-below struct: should not be traversed
            struct hidden_from_page {};
        };
    };

    /// A function to get a see-below symbol
    ///
    /// When used in a function, the symbol name should be shown as usual.
    /// The page for this symbol is what should be different because
    /// the synopsis should say "See below" and the members are not
    /// listed unless it's a namespace or the symbol has been explicitly
    /// used as a dependency elsewhere.
    see_below get_see_below() { return {}; }

    /// A symbol that passes the implementation-defined filter
    ///
    /// A symbol that passes the filters and the implementation-defined filter
    /// The symbol is implementation defined and should not have a page.
    /// Members of an implementation-defined scope should not be traversed.
    /// If they are traversed for some other reason, they should also become
    /// implementation-defined.
    struct implementation_defined {
        /// Child of an implementation-defined struct
        ///
        /// Child of an implementation-defined struct: should not be traversed
        /// and, if traversed for some other reason, should also be
        /// implementation-defined.
        struct also_implementation_defined {
            /// Grandchild of an implementation-defined struct
            ///
            /// Grandchild of an implementation-defined struct: should not be
            /// traversed and, if traversed for some other reason, should also
            /// be implementation-defined.
            struct hidden_from_page {};
        };
    };

    /// A function to get an implementation-defined symbol
    ///
    /// When used in a function, the implementation-defined
    /// comment should replace the real type.
    ///
    /// It's the responsibility of the function documentation
    /// to explain the implementation-defined symbol.
    implementation_defined get_implementation_defined() { return {}; }

    /// An excluded symbol used as a dependency by a regular symbol
    ///
    /// A symbol excluded by filters but is used as a dependency
    /// The symbol should be extracted as a dependency but its
    /// members should not be traversed.
    struct dependency {
        /// Child of a dependency struct
        ///
        /// Child of a dependency struct: should not be traversed
        /// and, if traversed for some other reason, should also be
        /// a dependency.
        struct hidden_from_page;
    };

    /// A function to get an excluded symbol
    ///
    /// When used in a function, only the symbol name should be shown.
    /// No links should be generated for this symbol.
    dependency get_dependency() { return {}; }

    /// A symbol excluded by filters
    ///
    /// A symbol excluded by filters and should not have a page.
    struct excluded {
        struct also_excluded {
            struct hidden_from_page {};
        };
    };
}

/// A see-below namespace
///
/// All member symbols should become see-below. All members are
/// traversed as see-below.
///
/// The documentation page for these symbols should include
/// the see-below comment.
namespace see_below_ns
{
    /// Regular symbol in a see-below namespace
    ///
    /// The symbol becomes see-below because the whole namespace
    /// is see-below.
    struct regular {
        /// Child of a regular symbol in a see-below namespace
        ///
        /// This symbol should not have a page because regular
        /// became see-below.
        struct also_regular {
            /// Grandchild of a regular symbol in a see-below namespace
            ///
            /// This symbol should not have a page because regular
            /// became see-below.
            struct regular_as_well {};
        };
    };

    /// See-below symbol in a see-below namespace
    ///
    /// The symbol becomes see-below because the whole namespace
    /// is see-below and because it's explicitly marked as see-below.
    struct see_below {};

    /// Implementation-defined symbol in a see-below namespace
    ///
    /// The symbol does not become see-below because the
    /// the implentation-defined filter has precedence over the
    /// see-below filter.
    ///
    /// Functions using this symbol should explain the implementation-defined
    /// nature of the symbol.
    struct implementation_defined {};

    /// A function to get an implementation-defined symbol in a see-below namespace
    ///
    /// When used in a function, the implementation-defined
    /// comment should replace the real type.
    ///
    /// It's the responsibility of the function documentation
    /// to explain the implementation-defined symbol.
    implementation_defined get_implementation_defined() { return {}; }

    /// A dependency symbol in a see-below namespace
    ///
    /// The symbol should be extracted as a dependency because the
    /// exclude filter has precedence over the see-below filter.
    /// Only included symbols can be promoted to see-below.
    ///
    /// This will not have a page and functions using this symbol
    /// should explain the dependency.
    struct dependency {};

    /// A function to get a dependency symbol in a see-below namespace
    ///
    /// The symbol should be extracted as a dependency because the
    /// exclude filter has precedence over the see-below filter.
    /// Only included symbols can be promoted to see-below.
    ///
    /// It's the responsibility of the function documentation
    /// to explain the dependency.
    dependency get_dependency() { return {}; }

    /// A symbol excluded by filters in a see-below namespace
    ///
    /// The symbol is excluded by filters and should not have a page.
    struct excluded {};
}

/// Namespace alias to form a dependency on the see-below namespace
///
/// The alias should be linked as usual and, because it's a namespace,
/// the members should be listed on the page.
namespace see_below_ns_alias = see_below_ns;

/// An implementation-defined namespace
///
/// Members are not traversed and, if traversed for some
/// other reason, they also become implementation-defined.
namespace implementation_defined_ns
{
    // Symbols directly filtered
    struct regular {};
    struct see_below {};
    struct implementation_defined {};
    struct dependency {};
    struct excluded {};

    dependency f() { return {}; }
}

/// Namespace alias to form a dependency on the implementation-defined namespace
namespace implementation_defined_ns_alias = implementation_defined_ns;

// A dependency namespace: all member symbols become dependencies
// Members should not be traversed and if they are, they should
// become dependencies.
namespace dependency_ns
{
    // Symbols directly filtered
    struct regular {};
    struct see_below {};
    struct implementation_defined {};
    struct dependency {};
    struct excluded {};

    dependency f() { return {}; }
}

/// Namespace alias to form the dependency on dependency_ns
namespace dependency_ns_alias = dependency_ns;

// An excluded namespace: all member symbols are excluded.
namespace excluded_ns
{
    // Symbols directly filtered
    struct regular {};
    struct see_below {};
    struct implementation_defined {};
    struct dependency {};
    struct excluded {};

    dependency f() { return {}; }
}

/// A regular symbol in the global namespace
///
/// This symbol should have a page as usual.
struct regular {
    /// Child of a regular symbol: should be traversed as usual
    struct also_regular {
        /// Grandchild of a regular symbol: should be traversed as usual
        struct regular_as_well {};
    };
};

/// A function to get a regular symbol in the global namespace
///
/// When used in a function, the symbol should be shown as usual
/// with a link to the page.
regular get_regular() { return {}; }

/// A see-below symbol in the global namespace
///
/// This symbol should have a page as usual but, because it's a scope
/// and not a namespace, the members should not be listed on that page.
///
/// The synopsis should say "See below".
struct see_below {
    /// Child of a see-below struct: should not be traversed
    /// and, if traversed for some other reason, should also be see-below.
    /// The symbol should not have a page.
    struct also_see_below {
        /// Grandchild of a see-below struct: should not be traversed
        /// and, if traversed for some other reason, should also be see-below.
        /// The symbol should not have a page.
        struct hidden_from_page {};
    };
};

/// A function to get a see-below symbol in the global namespace
///
/// When used in a function, the symbol name should be shown as usual.
/// The page for this symbol is what should be different because
/// the synopsis should say "See below" and the members are not
/// listed unless it's a namespace or the symbol has been explicitly
/// used as a dependency elsewhere.
see_below get_see_below() { return {}; }

/// An implementation-defined symbol in the global namespace
///
/// The symbol is implementation defined and should not have a page.
struct implementation_defined {};

/// A function to get an implementation-defined symbol in the global namespace
///
/// When used in a function, the implementation-defined
/// comment should replace the real type.
///
/// It's the responsibility of the function documentation
/// to explain the implementation-defined symbol.
implementation_defined get_implementation_defined() { return {}; }

/// A dependency symbol in the global namespace
struct dependency {};

/// A function to get a dependency symbol on the global namespace
///
/// The symbol should be extracted as a dependency but its
/// members should not be traversed.
dependency get_dependency() { return {}; }

/// An excluded symbol in the global namespace
struct excluded {};
