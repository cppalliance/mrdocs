#ifndef MRDOCS_TEST_FILES_INCLUDE_EXTERNAL_BASE_HPP
#define MRDOCS_TEST_FILES_INCLUDE_EXTERNAL_BASE_HPP

namespace test {

/// A base class declared in an out-of-tree header (outside source-root).
struct ExternalBase
{
    /// A member function inherited from the out-of-tree base.
    void externalMethod();
};

} // namespace test

#endif
