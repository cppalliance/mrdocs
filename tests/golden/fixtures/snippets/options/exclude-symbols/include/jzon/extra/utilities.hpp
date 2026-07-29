#ifndef JZON_EXTRA_UTILITIES_HPP
#define JZON_EXTRA_UTILITIES_HPP

// tag::body[]
namespace jzon::extra {

// Filtered out: `exclude-symbols: 'jzon::extra::**'` drops these
// symbols by qualified name, so the `jzon::extra` namespace and its
// members never enter the rendered docs.
void enable_trace();
void disable_trace();

}
// end::body[]

#endif
