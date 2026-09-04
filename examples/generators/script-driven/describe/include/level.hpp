#ifndef APP_LEVEL_HPP
#define APP_LEVEL_HPP

namespace app {

/// Logging severity.
enum class Level
{
    debug,    ///< Verbose tracing.
    info,     ///< Normal operation.
    warning,  ///< Something looks wrong.
    error     ///< A failure occurred.
};

} // namespace app

#endif
