/// A logger mixin.
class logger
{
public:
    /// Write a message to the log.
    void log(const char* msg);
};

/// A widget that privately inherits from `logger`
/// for the logging implementation.
class widget : private logger
{
public:
    /// Render the widget and write a log entry.
    void show();
};
