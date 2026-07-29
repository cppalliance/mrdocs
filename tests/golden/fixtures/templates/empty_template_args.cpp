// A class template with a default template argument, used as a
// function return type via the empty-argument-list specialization
// `Task<>`. The rendered output should preserve the angle brackets
// (`Task<>`), not collapse to `Task`. See issue #1184.

template <typename T = int>
struct Task {};

Task<> handshake();
