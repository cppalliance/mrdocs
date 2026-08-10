namespace net {

/** A move-only handle to an open TCP connection.

    Obtain one from `connect`; it closes the socket when it goes out of scope.
    Send and receive through the free functions in this namespace:

    @code
    auto c = net::connect("example.org", 80);
    net::write(c, request);
    @endcode

    Its representation is an implementation detail that changes between releases,
    so the definition is not shown here; use it only through the operations
    described above.

    @seebelow
*/
class connection
{
    int fd_;
public:
    /// Close the connection.
    void close();
};

}
