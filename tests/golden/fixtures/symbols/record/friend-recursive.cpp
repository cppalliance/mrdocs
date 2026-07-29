// Minimal repro for issue #1117: recursive friend declarations in templates
// previously caused unbounded traversal and a crash in TagType printing.

namespace repro
{
template<class T>
struct FriendLoop
{
    // Each instantiation friends every other instantiation, creating a dense
    // friend graph that used to trigger recursive traversal.
    template<class U>
    friend struct FriendLoop;
};
} // namespace repro
