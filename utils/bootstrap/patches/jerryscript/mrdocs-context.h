/* Force-included into every jerry-core translation unit by the patched
 * CMakeLists.txt next to this file (-include on GCC and Clang, /FI on MSVC).
 *
 * With JERRY_EXTERNAL_CONTEXT the engine reads its context through
 * jerry_port_context_get() several times per bytecode instruction, and an
 * out-of-line call into the host is the one thing the host cannot make
 * cheaper. This header turns every such read into a load of a thread-local
 * pointer the host defines. The port header is included first, so its
 * declaration of jerry_port_context_get compiles normally; the macro then
 * rewrites the engine's calls only. The host keeps the variable equal to
 * what its jerry_port_context_get returns, so an engine built without this
 * header behaves the same, just slower. */
#ifndef MRDOCS_JERRY_CONTEXT_H
#define MRDOCS_JERRY_CONTEXT_H

#include "jerry-core/include/jerryscript-port.h"

#if defined(_MSC_VER)
extern __declspec(thread) struct jerry_context_t *jerry_port_context_tls;
#else
extern __thread struct jerry_context_t *jerry_port_context_tls;
#endif

#define jerry_port_context_get() (jerry_port_context_tls)

#endif /* MRDOCS_JERRY_CONTEXT_H */
