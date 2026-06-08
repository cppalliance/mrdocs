#ifndef CLI_DETAIL_TOKENS_HPP
#define CLI_DETAIL_TOKENS_HPP
// tag::body[]
namespace cli::detail {
// Filtered out: lives in a subdirectory that `recursive: false`
// skips, so `cli::detail::tokenize` never enters the corpus.
void tokenize(char const* arg);
}
// end::body[]
#endif
