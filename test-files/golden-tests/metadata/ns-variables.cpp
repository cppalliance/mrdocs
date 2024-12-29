// variables and constants

constexpr int i = 0;
constinit int j = 0;

extern const int k;
extern int l;

constexpr int k = 1;
constinit int l = 1;

double pi = 3.14;

struct T {};

extern T t;

thread_local int x0 = 0;
thread_local static int x1 = 0;
constexpr thread_local static int x2 = 0;
