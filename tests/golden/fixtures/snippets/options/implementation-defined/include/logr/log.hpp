#ifndef LOGR_LOG_HPP
#define LOGR_LOG_HPP

#include <logr/detail/scope_token.hpp>

// tag::body[]
namespace logr {

/** Attach a key/value pair to every log line in the current scope.

    The context stays attached until the returned token is destroyed.
    Hold it in `auto`; nested contexts stack and unwind in reverse order.

    @par Example
    @code
    void handle_request(request const& req) {
        auto ctx = logr::scoped_context("request_id", req.id);
        do_work(req);
    }
    @endcode

    @param key   The context name surfaced on each log line.
    @param value The context value surfaced on each log line.
    @return An opaque RAII token whose lifetime governs how long the pair
            stays attached. The type is implementation-defined; store it
            in `auto`.
*/
detail::scope_token scoped_context(char const* key, char const* value);

}
// end::body[]

#endif
