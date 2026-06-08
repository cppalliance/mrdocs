#ifndef ZLIB_H
#define ZLIB_H
// tag::body[]

// Vendored copy of zlib's public API. Documented upstream at
// https://zlib.net/, not here. `exclude: include/zlib` keeps the
// vendored library out of the rendered docs so it does not appear
// alongside the httpd API.

/* Compress `src` into `dst` using DEFLATE. */
int compress2(unsigned char* dst, unsigned long* dstLen,
              unsigned char const* src, unsigned long srcLen,
              int level);

/* Decompress a DEFLATE-compressed buffer. */
int uncompress(unsigned char* dst, unsigned long* dstLen,
               unsigned char const* src, unsigned long srcLen);
// end::body[]
#endif
