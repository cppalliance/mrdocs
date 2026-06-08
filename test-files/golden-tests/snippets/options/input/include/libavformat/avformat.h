#ifndef LIBAVFORMAT_AVFORMAT_H
#define LIBAVFORMAT_AVFORMAT_H
// tag::body[]
// Filtered out: `input: include/libavcodec` scopes extraction to that
// directory, so the format-library functions never enter the corpus.
// The directory matters because both libraries put their declarations
// at global scope under the same `av_*` prefix, so a name-based filter
// could not distinguish them.
int av_open_input_file(void);
void av_close_input_file(void);
// end::body[]
#endif
