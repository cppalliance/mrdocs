class C {
public:
    int foo();
    int foo(bool);
    static int foo(int);
    static int foo(int, bool);
protected:
    int foo(int, int);
    int foo(int, int, bool);
    static int foo(int, int, int);
    static int foo(int, int, int, bool);
private:
    int foo(int, int, int, int);
    int foo(int, int, int, int, bool);
    static int foo(int, int, int, int, int);
    static int foo(int, int, int, int, int, bool);
};
