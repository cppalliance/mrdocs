
#include "SymbolSet.hpp"

namespace mrdocs {

std::size_t
SymbolPtrHasher::
operator()(
    const std::unique_ptr<Symbol>& I) const
{
    // The info set should never contain nullptrs
    MRDOCS_ASSERT(I);
    return std::hash<SymbolID>()(I->id);
}

std::size_t
SymbolPtrHasher::
operator()(
    SymbolID const& id) const
{
    return std::hash<SymbolID>()(id);
}

bool
SymbolPtrEqual::
operator()(
    const std::unique_ptr<Symbol>& a,
    const std::unique_ptr<Symbol>& b) const
{
    MRDOCS_ASSERT(a && b);
    if(a == b)
    {
        return true;
    }
    return a->id == b->id;
}

bool
SymbolPtrEqual::
operator()(
    const std::unique_ptr<Symbol>& a,
    SymbolID const& b) const
{
    MRDOCS_ASSERT(a);
    return a->id == b;
}

bool
SymbolPtrEqual::
operator()(
    SymbolID const& a,
    const std::unique_ptr<Symbol>& b) const
{
    MRDOCS_ASSERT(b);
    return b->id == a;
}

} // mrdocs
