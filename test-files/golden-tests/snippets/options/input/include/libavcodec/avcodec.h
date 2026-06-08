#ifndef LIBAVCODEC_AVCODEC_H
#define LIBAVCODEC_AVCODEC_H
// tag::body[]
/** Allocate a new packet on the heap. */
int av_packet_alloc(void);

/** Release a packet allocated with `av_packet_alloc`. */
void av_packet_free(void);
// end::body[]
#endif
