#ifndef HTTPD_SERVER_HPP
#define HTTPD_SERVER_HPP
// tag::body[]
namespace httpd {

/** Start the HTTP server on `port`. */
void start(int port);

/** Stop the running server. */
void stop();

}
// end::body[]
#endif
