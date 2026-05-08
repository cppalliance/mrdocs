// Regression test for the crash described in
// https://github.com/cppalliance/mrdocs/issues/1195.
//
// Earlier versions of `checkUndocumented` called `undocumented_.erase(it)`
// without checking `it != end()`. With `extract-all: false` and a documented
// symbol, the lookup misses (the set is empty under that config) and the
// unchecked erase is undefined behaviour. `warn-if-undocumented` must be left
// at its default true here so the buggy block is reached.

/// A documented struct.
struct Documented {};

struct Undocumented {};

/// A documented function.
void documentedFunction();

void undocumentedFunction();
