// Coverage for see-below bases retaining inherited documentation.
namespace see_below_ns {

struct base_see {
    /// Describes the see-below member
    void do_other();
};

} // see_below_ns

struct derived_see : see_below_ns::base_see
{
    void own();
};
