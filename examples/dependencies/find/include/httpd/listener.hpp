#ifndef HTTPD_LISTENER_HPP
#define HTTPD_LISTENER_HPP

#include <boost/asio/ip/tcp.hpp>

namespace httpd {

/// A TCP listener bound to a specific endpoint.
class Listener
{
public:
    /// Bind the listener to the given endpoint.
    explicit Listener(boost::asio::ip::tcp::endpoint const& ep);

    /// Return the endpoint the listener is bound to.
    boost::asio::ip::tcp::endpoint const& endpoint() const;

private:
    boost::asio::ip::tcp::endpoint endpoint_;
};

} // namespace httpd

#endif
