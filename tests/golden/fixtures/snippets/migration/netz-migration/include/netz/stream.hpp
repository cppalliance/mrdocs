#include <netz/buffer_view.hpp>
namespace netz {
#if defined(NETZ_DOCUMENTATION_BUILD)
/** Locate the first complete packet at the start of the buffer.

    Scans `input` for a frame boundary. The returned view aliases the
    same memory as `input`, so the storage behind `input` must outlive
    the returned view.

    @param input Bytes received from the stream, up to the caller's
                 current fill mark.
    @return A view of the bytes belonging to the first packet, or an
            empty view if no complete packet is present yet.
*/
__see_below__ first_packet(__see_below__ input);
#else
buffer_view first_packet(buffer_view input);
#endif
} // namespace netz
