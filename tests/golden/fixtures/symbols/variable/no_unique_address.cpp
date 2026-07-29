// data members

struct Empty {};

struct T
{
    int i;
    [[/*no_unique_address, */deprecated, maybe_unused]] Empty e = Empty{};
};
// https://devblogs.microsoft.com/cppblog/msvc-cpp20-and-the-std-cpp20-switch/
