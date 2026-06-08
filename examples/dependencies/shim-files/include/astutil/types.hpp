#ifndef ASTUTIL_TYPES_HPP
#define ASTUTIL_TYPES_HPP

#include <llvm/ADT/StringRef.h>

namespace astutil {
/// Look up a symbol by its internal id and return its mangled name.
llvm::StringRef const& symbol_name(int symbol_id);
} // namespace astutil

#endif
