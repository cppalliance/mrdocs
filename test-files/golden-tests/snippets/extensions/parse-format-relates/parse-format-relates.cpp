/// An HTTP request as a structured value.
struct request;

/// Parse `text` into a request. Returns a valid request on success.
request parse_request(char const* text);

/// Format `r` as the wire-format text of an HTTP request.
char const* format_request(request const& r);

/// A user record.
struct user;

/// Parse `text` into a user.
user parse_user(char const* text);

/// Format `u` as the canonical wire-format text of a user.
char const* format_user(user const& u);
