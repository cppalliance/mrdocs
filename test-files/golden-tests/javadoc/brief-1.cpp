// Test the various ways the
// brief can be specified

#if 0
/**brief
*/
void f1();

/** brief
*/
void f2();

/** brief
continued
*/
void f3();

/** pre

    @brief brief

    post
*/
void f4();
#endif

/** brief @b bold it
 continues to the line.
*/
void f5();

/** brief

    many lined
    @b bold
    what will
    happen?
*/
void f6();
