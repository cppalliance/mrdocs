#include <httpd/listener.hpp>

namespace httpd {

Listener::Listener(boost::asio::ip::tcp::endpoint const& ep)
    : endpoint_(ep)
{
}

boost::asio::ip::tcp::endpoint const&
Listener::endpoint() const
{
    return endpoint_;
}

} // namespace httpd
