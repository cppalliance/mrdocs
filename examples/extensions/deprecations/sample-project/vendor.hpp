// A header of another library: the sample project includes it but does not
// document it, so what it deprecates is not part of the report.
namespace vendor {

struct Base
{
    [[deprecated("not ours")]]
    void legacy();
};

struct [[deprecated("not ours either")]] Old
{
};

} // namespace vendor
