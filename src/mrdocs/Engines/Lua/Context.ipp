// Impl fragment of Lua.cpp (one TU): the lua::Context special members and nativeState.
// Included within `namespace mrdocs::lua {`. Not a standalone header.

//------------------------------------------------

Context::~Context() = default;

Context::
Context()
    : impl_(std::make_shared<Impl>())
{
}

Context::
Context(
    Context const& other) noexcept = default;

void*
Context::
nativeState() const noexcept
{
    return impl_->L;
}
