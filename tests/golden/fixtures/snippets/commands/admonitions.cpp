/** Closes the handle.

    @note Closing a handle invalidates any iterators
    that point into the underlying buffer.

    @warning Do not call this from a destructor that
    runs during program termination.

    @tip Prefer the RAII wrapper `scoped_handle`,
    which closes automatically at scope exit.
 */
void close_handle();
