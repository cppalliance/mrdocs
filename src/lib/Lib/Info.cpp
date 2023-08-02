
#include "Info.hpp"

namespace clang {
namespace mrdox {

std::size_t
InfoPtrHasher::
operator()(
    const std::unique_ptr<Info>& I) const
{
    // the info set should never contain nullptrs
    MRDOX_ASSERT(I);
    return std::hash<SymbolID>()(I->id);
}

std::size_t
InfoPtrHasher::
operator()(
    const SymbolID& id) const
{
    return std::hash<SymbolID>()(id);
}

bool
InfoPtrEqual::
operator()(
    const std::unique_ptr<Info>& a,
    const std::unique_ptr<Info>& b) const
{
    MRDOX_ASSERT(a && b);
    if(a == b)
        return true;
    return a->id == b->id;
}

bool
InfoPtrEqual::
operator()(
    const std::unique_ptr<Info>& a,
    const SymbolID& b) const
{
    MRDOX_ASSERT(a);
    return a->id == b;
}

bool
InfoPtrEqual::
operator()(
    const SymbolID& a,
    const std::unique_ptr<Info>& b) const
{
    MRDOX_ASSERT(b);
    return b->id == a;
}

} // mrdox
} // clang
