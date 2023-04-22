// member function qualifiers

struct T01 { void f(); };
struct T02 { static void f(); };
struct T03 { void f() &; };
struct T04 { void f() &&; };
struct T05 { void f() const; };
struct T06 { constexpr void f() {} };
//struct T07 { consteval void f() {} };
struct T08 { inline void f(); };
struct T09 { void f() noexcept; };
struct T10 { auto f() -> void; };
struct T11 { auto f() -> int; };
struct T12 { void f(...); };
struct T13 { virtual void f(); };
struct T14 { virtual void f() = 0; };
struct T15 { void f() volatile; };
struct T16 { static void f(); };

struct T17 : T14 { void f() override; };

struct U
{
    inline constexpr auto f1(...) volatile const noexcept -> void {}
    static inline constexpr auto f2() noexcept -> char { return ' '; }
    virtual int f3() const volatile noexcept = 0;
};
