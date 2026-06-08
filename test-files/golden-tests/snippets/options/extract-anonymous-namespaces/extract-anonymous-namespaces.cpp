namespace lib {

/// A regular public function.
void run();

namespace {
/// A helper with translation-unit-local linkage. With
/// `extract-anonymous-namespaces: true` it shows up
/// in the docs.
void warm_caches();
}

}
