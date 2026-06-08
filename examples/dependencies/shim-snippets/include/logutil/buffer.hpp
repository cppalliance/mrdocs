#ifndef LOGUTIL_BUFFER_HPP
#define LOGUTIL_BUFFER_HPP

#include <fmt/format.h>

namespace logutil {
/// Append `text` to a {fmt} memory buffer.
void append(fmt::memory_buffer& out, char const* text);
} // namespace logutil

#endif
