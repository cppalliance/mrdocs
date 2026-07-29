// Repro for cppalliance/mrdocs#1107: documentation carried from hidden bases.
namespace detail {

struct base_detail {
    /// Do the thing
    void do_it();
};

} // detail

struct derived : detail::base_detail
{
    void own();
};
