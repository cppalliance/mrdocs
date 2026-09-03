// Implicit specializations and the documented surface.
//
// An implicit specialization is what the compiler produces when a template is
// used with concrete arguments. Nobody wrote it, so the primary template is
// its documentation, and it must not appear in the output on its own: a
// reader looking for `Manager<Module>` reads `Manager`.
//
// Expected in the output: `Manager` (the primary) with its member
// `Invalidator` and `clear`, `Module`, `Function`, `Kernel`, the function
// `use`, the alias `FunctionManager`, and exactly one specialization of
// `Manager`, the one for `Kernel`, holding its specialized `clear`.
//
// Not expected: `Manager<Module>`, `Manager<Function>`, or any member of
// theirs.

/// A manager.
/// @tparam IRUnitT IR unit type.
template <typename IRUnitT>
struct Manager {
    /// Invalidates analyses.
    struct Invalidator {
        /// Invalidate.
        /// @param IR The IR unit.
        /// @return Whether anything was invalidated.
        bool invalidate(IRUnitT &IR);
    };

    /// Clear the cache.
    void clear();
};

/// A module.
struct Module {};

/// A function.
struct Function {};

/// A kernel.
struct Kernel {};

// Case 1: an implicit specialization reached as the type of a parameter.
// Traversing the parameter type leads to `Manager<Module>::Invalidator`, and
// from there to its parent `Manager<Module>`. Both are dependencies: neither
// gets a page, and the parameter type in the synopsis of `use` links to the
// primary `Manager`. Before the fix, both came out as regular symbols with
// every instantiated member, and one could be reported as undocumented even
// though there is no declaration to put a comment on.

/// Uses a nested type of an implicit specialization as a parameter type.
/// @param Inv The invalidator.
void use(Manager<Module>::Invalidator &Inv);

// Case 2: an implicit specialization reached through a typedef. The alias is
// documented and appears; `Manager<Function>`, its target, is a dependency and
// the alias's type resolves to the primary.

/// Names an implicit specialization through a typedef.
using FunctionManager = Manager<Function>;

// Case 3, the exception: a written declaration inside an implicit
// specialization. `Manager<Kernel>::clear` is an explicit specialization of a
// member, written and documented by the user, so it must appear. Its parent
// `Manager<Kernel>` is given a place in the output to hold it; this is the
// only specialization of `Manager` that appears.

/// The kernel manager clears differently.
template <>
void Manager<Kernel>::clear();
