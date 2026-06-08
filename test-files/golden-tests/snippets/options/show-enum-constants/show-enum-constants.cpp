/// Status of a network operation.
enum class status
{
    /// Operation succeeded.
    ok,
    /// Operation timed out.
    timeout,
    /// Operation failed for another reason.
    error,
};
