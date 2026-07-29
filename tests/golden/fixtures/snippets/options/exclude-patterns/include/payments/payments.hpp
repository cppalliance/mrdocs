#ifndef PAYMENTS_PAYMENTS_HPP
#define PAYMENTS_PAYMENTS_HPP
// tag::body[]
namespace payments {

/** Authorize a payment of `amount` against `account`. */
bool authorize(char const* account, double amount);

}
// end::body[]
#endif
