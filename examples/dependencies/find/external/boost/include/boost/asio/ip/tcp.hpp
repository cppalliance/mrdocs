#ifndef BOOST_ASIO_IP_TCP_HPP
#define BOOST_ASIO_IP_TCP_HPP

// This file vendors the minimum of Boost.Asio that `httpd` references.
// In a real project, BOOST_ROOT would point at the full Boost install
// and this directory would not exist in the project source tree.

namespace boost {
namespace asio {
namespace ip {

class tcp
{
public:
    /// A TCP endpoint: an address paired with a port.
    class endpoint
    {
    public:
        endpoint() = default;
        explicit endpoint(unsigned short port) noexcept : port_(port) {}
        unsigned short port() const noexcept { return port_; }
    private:
        unsigned short port_ = 0;
    };
};

} // namespace ip
} // namespace asio
} // namespace boost

#endif
