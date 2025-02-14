namespace empty_ns {}

// Symbols are excluded so the namespace should also be excluded
namespace excluded_members_ns {
    struct ExcludedStructA {};
    struct ExcludedStructB {};
}

/// Regular namespace
namespace regular_ns {
    struct A {};
}

/// Namespace documentation
namespace documented_ns {}

/// Should decay to see-below
namespace see_below_ns {
    struct SeeBelowStructA {};
    struct SeeBelowStructB {};
}

/// Should decay to implementation-defined
namespace implementation_defined_ns {
    struct ImplementationDefinedStructA {};
    struct ImplementationDefinedStructB {};
}

/// Should decay to see-below
namespace mixed_ns {
    struct SeeBelowStructA {};
    struct ImplementationDefinedStructB {};
}

/// Should decay to regular
namespace mixed_regular_ns {
    struct RegularStructA {};
    struct SeeBelowStructB {};
    struct ImplementationDefinedStructC {};
}

/// Should work
using namespace regular_ns;

/// Should work
namespace regular_ns_alias = regular_ns;

/// Should work
using namespace empty_ns;

/// Should still work
namespace empty_ns_alias = empty_ns;

