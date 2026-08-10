/** Exit the program.

    The program will end immediately.

    @note Functions registered with `std::atexit` are not invoked.
*/
[[noreturn]]
void
terminate() noexcept;