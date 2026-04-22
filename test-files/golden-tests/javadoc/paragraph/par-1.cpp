/** Brief

    @par Custom par
    Paragraph 1

    @code
    void f1();
    @endcode

 */
void f1();

/** Brief

    @par Custom par

    Paragraph 2

    @code
    void f2();
    @endcode

 */
void f2();

/** Brief

    @par Custom par
    @code
    void f3();
    @endcode
 */
void f3();

/** Brief

    @par Custom par

    @code
    void f4();
    @endcode
 */
void f4();

// No front matter: the body of @par must not be promoted
// to an auto-brief (issue #1162).
/**

    @par Custom par
    Paragraph 5
 */
void f5();

// Same shape as f5 but with explicit front matter: the front
// matter becomes the brief, the @par section is preserved.
/** A function.

    @par Custom par
    Paragraph 6
 */
void f6();