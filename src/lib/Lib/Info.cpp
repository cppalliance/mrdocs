
#include "Info.hpp"

namespace clang {
namespace mrdocs {

std::size_t
InfoPtrHasher::
operator()(
    const std::unique_ptr<Info>& I) const
{
    // The info set should never contain nullptrs
    MRDOCS_ASSERT(I);
    return std::hash<SymbolID>()(I->id);
}

std::size_t
InfoPtrHasher::
operator()(
    SymbolID const& id) const
{
    return std::hash<SymbolID>()(id);
}

bool
InfoPtrEqual::
operator()(
    const std::unique_ptr<Info>& a,
    const std::unique_ptr<Info>& b) const
{
    MRDOCS_ASSERT(a && b);
    if(a == b)
    {
        return true;
    }
    return a->id == b->id;
}

bool
InfoPtrEqual::
operator()(
    const std::unique_ptr<Info>& a,
    SymbolID const& b) const
{
    MRDOCS_ASSERT(a);
    return a->id == b;
}

bool
InfoPtrEqual::
operator()(
    SymbolID const& a,
    const std::unique_ptr<Info>& b) const
{
    MRDOCS_ASSERT(b);
    return b->id == a;
}

} // mrdocs
} // clang
