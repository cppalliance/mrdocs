// Impl fragment of JavaScript.cpp (one TU): the js::Context special members.
// Included within `namespace mrdocs::js {`. Not a standalone header.

Context::Context() : impl_(std::make_shared<Impl>())
{
    impl_->context_refs.store(1, std::memory_order_relaxed);
}

Context::Context(Context const& other) noexcept
    : impl_(other.impl_)
{
    if (impl_)
    {
        impl_->context_refs.fetch_add(1, std::memory_order_relaxed);
    }
}

Context::~Context()
{
    // Tear down the JerryScript context only when the last Context sharing this
    // Impl goes away (context_refs counts live Context instances and copies).
    // DomValueHolder objects keep a shared_ptr<Impl>, so the interpreter owns
    // them through a reference cycle; cleanup() breaks that cycle by tearing
    // down the holders. Doing it on every ~Context (regardless of remaining
    // references) tore the engine down early for surviving copies and left the
    // cycle unbroken for the copies the transforms/generators hold, leaking the
    // whole interpreter heap.
    if (impl_ &&
        impl_->context_refs.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        impl_->cleanup();
    }
}
