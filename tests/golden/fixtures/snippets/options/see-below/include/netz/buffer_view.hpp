#ifndef NETZ_BUFFER_VIEW_HPP
#define NETZ_BUFFER_VIEW_HPP

// tag::body[]
namespace netz {

/** A non-owning view of a contiguous byte range.

    Used as the parameter and return type for the framing helpers in
    `netz::stream` and as the buffer surface presented to async read
    handlers. Stores a pointer and a length, and does not extend the
    lifetime of the storage it refers to.
*/
class buffer_view
{
public:
    unsigned char const* data() const noexcept;
    unsigned long size() const noexcept;
};

}
// end::body[]

#endif
