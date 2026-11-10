namespace logr {

/// Attach a key/value pair to every log line in the current scope.
void scoped_context(char const* key, char const* value);

} // namespace logr
