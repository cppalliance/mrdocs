/** Moves funds from one account to another.

    @pre `amount` is positive.
    @pre `from` and `to` are different accounts.
    @post `from` falls by `amount` and `to` rises by the same.
    @throws std::invalid_argument If an account number is unknown.
    @throws std::out_of_range If `from` cannot cover `amount`.
 */
void transfer(int from, int to, long amount);
