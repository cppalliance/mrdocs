#include <filesystem>

namespace detail {

/** The object that holds the lock.

    @implementationdefined
*/
class file_lock
{
public:
    ~file_lock();
};

} // namespace detail

/** Lock a file for exclusive access.

    The lock is held until the returned object is destroyed.

    @param path The path of the file to lock.
    @return An object that holds the lock.
    @throws std::system_error If the file cannot be locked.
*/
[[nodiscard]]
detail::file_lock
lock_file(std::filesystem::path const& path);
