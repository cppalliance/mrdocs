struct thingy {};

namespace nested_detail
{

struct thingy_impl {};

}

namespace other
{
struct thingy { };


namespace nested_detail {

struct thingy_impl {};

namespace moar_detail {}

}

}