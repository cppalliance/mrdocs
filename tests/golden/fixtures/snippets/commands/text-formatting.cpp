/** Formats a quantity for *human* reading.

    Returns a **localized** string. Pass the raw value in `cents`;
    the ~~deprecated~~ `format_dollars` spelling is gone. Out-of-range
    inputs are ==flagged== in the output.
 */
char const* format_money(long cents);
