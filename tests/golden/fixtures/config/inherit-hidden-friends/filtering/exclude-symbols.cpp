namespace lib {
namespace detail {

// Carries the operators for every `value`. A CRTP/deducing-`this` base like
// this is the only way to declare them once for all specializations.
struct value_ops {
  template <class T>
  friend constexpr bool operator==(value_ops const&, T const&) { return true; }
};

}  // namespace detail

/// A value.
template <class T>
class value : public detail::value_ops {
public:
  /// Construct from a raw T.
  explicit constexpr value(T raw) : raw_(raw) {}
private:
  T raw_;
};

}  // namespace lib
