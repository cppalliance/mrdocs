#ifndef NETZ_STREAM_HPP
#define NETZ_STREAM_HPP

#include <netz/buffer_view.hpp>

// tag::body[]
namespace netz {

/** Locate the first complete packet at the start of the buffer.

    Scans `input` for a frame boundary. The returned view aliases the
    same memory as `input`, so the storage behind `input` must outlive
    the returned view.

    @param input Bytes received from the stream, up to the caller's
                 current fill mark.
    @return A view of the bytes belonging to the first packet, or an
            empty view if no complete packet is present yet.
*/
buffer_view first_packet(buffer_view input);

}
// end::body[]

#endif
