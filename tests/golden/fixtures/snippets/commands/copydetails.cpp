/** @brief Logs a deprecation warning for the given symbol.

    Emits a one-time warning to the diagnostic stream
    when a deprecated symbol is used. The warning includes
    the symbol name and the recommended replacement.
 */
void deprecation_notice(const char* name);

/** @brief Logs an info-level message.

    @copydetails deprecation_notice
 */
void info_log(const char* msg);
