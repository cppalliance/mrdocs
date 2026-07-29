struct T
{
    using U1 = int;
    typedef char U2;

    void f1();
    int f2();
    char f3();

protected:
    /** brief-g1

        desc
    */
    void g1();

    /** brief-g2

        @return the number 2
    */
    int g2();

    /** brief-g3

        @return the separator
        @param x any old number
    */
    char g3(int x);
};
