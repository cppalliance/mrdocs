/** A handle whose exact representation is unspecified.

    @seebelow
 */
class handle
{
    int fd_;
public:
    /** Close the handle. */
    void close();
};
