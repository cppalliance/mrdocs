/// This namespace should extracted because
/// it's implied by `N0::f0_WL`
namespace N0
{
    /// This function should be included because
    /// it matches `N0::f0_WL`
    void f0_WL();

    /// This function should be excluded because
    /// it doesn't match `N0::f0_WL`
    void f1_BL();
}

/// This namespace should extracted because
/// it's implied by `N1::N3_WL` and `N1::N4::f1_WL`
namespace N1
{
    /// This function should be excluded because
    /// it doesn't match `N1::N3_WL` or `N1::N4::f1_WL`
    void f0_BL();

    /// This namespace should be excluded because
    /// it doesn't match `N1::N3_WL` or `N1::N4::f1_WL`
    namespace N2
    {
        /// This function should be excluded because
        /// it doesn't match `N1::N3_WL` or `N1::N4::f1_WL`
        void f1_BL();
    }

    /// This namespace should extracted because
    /// it's explicitly included by `N1::N3_WL`
    namespace N3_WL
    {
        /// This function should extracted because
        /// the namespace `N1::N3_WL` is included
        /// as a literal.
        void f1_WL();
    }

    /// This namespace should extracted because
    /// it's implied by `N1::N4::f1_WL`
    namespace N4
    {
        /// This function should extracted because
        /// it matches `N1::N4::f1_WL`
        void f1_WL();

        /// This function should be excluded because
        /// it doesn't match `N1::N4::f1_WL`
        void f2_BL();
    }
}

/// This namespace should extracted because
/// it's implied by `N5::N6::*7`
namespace N5
{
    /// This function should be excluded because
    /// it doesn't match `N5::N6::*7`
    void f0();

    /// This namespace should extracted because
    /// it's implied by `N5::N6::*7`
    namespace N6
    {
        /// This function should be excluded because
        /// it doesn't match `N5::N6::*7`
        void f1_BL();

        /// This namespace should be included because
        /// it matches `N5::N6::*7`
        namespace N7
        {
            /// This function should be included because
            /// it's a member of `N7`, which matches
            /// `N5::N6::*7`
            void f2_WL();
        }

        /// This namespace should be included because
        /// it matches `N5::N6::*7`
        namespace M7
        {
            /// This function should be included because
            /// it's a member of `M7`, which matches
            /// `N5::N6::*7`
            void f2_WL();
        }

        /// This namespace should be excluded because
        /// it doesn't match or is implied by
        /// `N5::N6::*7`
        namespace N8
        {
            /// This function should be excluded because
            /// it doesn't match `N5::N6::*7`
            void f2_BL();
        }
    }
}

/// This namespace should be included because
/// it strictly matches `C`
struct C
{
    /// This function should be included because
    /// it's a member of `C`
    void f0_WL();

    /// This struct should be included because
    /// it's a member of `C`
    struct D {
        /// This function should be included because
        /// it's a member of `D`
        void f1_WL();
    };
};

/// This namespace should be excluded even
/// though it matches the prefix of `N9::*_WL`
/// because there are no members of `N9` that
/// actually match the filters.
namespace N9
{
    /// This function should be excluded because
    /// it doesn't match `N9::*_WL`
    void f0_BL();
}