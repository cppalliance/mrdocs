# tests/integration/

Suites that use the built tool or the installed package from outside, as a user does. Every test carries the `integration` label.

They are not unit tests. The unit suite is one executable that globs `tests/unit/` and calls the API in process. These suites build separate shared libraries or consumer projects, run the `mrdocs` tool or a program linked against the package, and check its exit status and messages.

* `plugin-api/` builds plugins, loads them into the tool and checks what the loader accepts and refuses. See its [README](plugin-api/README.md).
* `cmake/` is a consumer project that finds an installed MrDocs with `find_package(mrdocs)`. See its [README](cmake/README.md).

Run the suites the build registers with the `integration` label, as `docs/modules/ROOT/pages/contribute/testing.adoc` shows. You configure the consumer project by hand against an install prefix.
