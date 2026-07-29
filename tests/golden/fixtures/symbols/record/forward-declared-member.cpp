namespace std {
  class path {
    struct _Cmpt;
  };
  struct path::_Cmpt : path {};
} // namespace std
namespace fmt {
  struct path : std::path {};
} // namespace fmt
