/// A stand-in for a stream type used by the extraction operator.
struct istream;

/// A class template with two unrelated `operator>>`.
///
/// Reduced from Boost.DynamicBitset: a member bitwise right-shift
/// operator and a friend stream extraction operator. The friend
/// declaration is essential to trigger the anchor collision (#919),
/// where both operators were assigned the same AsciiDoctor section id.
template <class Block>
class dynamic_bitset
{
public:
    /// Bitwise right-shift operator (member).
    ///
    /// \param n The shift amount.
    /// \return Reference to the current object.
    dynamic_bitset& operator>>(int n);

    /// Stream extraction operator (friend / free function).
    ///
    /// \param is The input stream.
    /// \param bs The bitset to read into.
    /// \return Reference to the input stream.
    template <class B>
    friend istream& operator>>(istream& is, dynamic_bitset<B>& bs);
};
