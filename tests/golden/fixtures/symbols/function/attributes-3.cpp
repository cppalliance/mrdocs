// A deprecated function with a message, and an attribute with multiple arguments.
[[deprecated("use compute2")]] int compute();

[[gnu::format(printf, 1, 2)]] void log_message(char const* fmt, ...);

// The same attribute repeated with different arguments is kept, not deduplicated.
[[clang::annotate("a"), clang::annotate("b")]] void annotated();

// A raw string literal with an inner comma stays a single argument
// (printPretty renders it as a normal string literal).
[[deprecated(R"(use a, b)")]] void raw_message();
