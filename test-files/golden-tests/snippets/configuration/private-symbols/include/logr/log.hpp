#ifndef LOGR_LOG_HPP
#define LOGR_LOG_HPP

#include <logr/detail/scope_token.hpp>

// tag::body[]
namespace logr {

/// The library's private encoder representation.
class encoder_impl;

/** A complete log record.

    Carries the message text and the level at which the record was
    emitted. Construct one and hand it to @ref emit().
*/
class log_record
{
public:
    log_record(char const* message) noexcept;

    char const* message() const noexcept;
    char const* level_name() const noexcept;

private:
    char const* message_;
    int level_;
};

/** Attach a key/value pair to every log line in the current scope.

    The context stays attached until the returned token is destroyed.
    Hold it in `auto`; nested contexts stack and unwind in reverse order.

    @param key   The context name surfaced on each log line.
    @param value The context value surfaced on each log line.
    @return An opaque RAII token whose lifetime governs how long the
            pair stays attached.
*/
detail::scope_token scoped_context(char const* key, char const* value);

/** Replace the active encoder.

    Obtain an encoder via @ref make_json_encoder() or
    @ref make_text_encoder() and hand it here.

    @par Example
    @code
    logr::set_encoder(logr::make_json_encoder());
    @endcode
*/
void set_encoder(encoder_impl&& enc);

}
// end::body[]

#endif
