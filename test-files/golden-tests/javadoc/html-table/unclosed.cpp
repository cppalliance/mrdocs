/** Function whose doc comment contains an unclosed table tag.

    The parser warns and skips the table:

    <table>
 */
void f1();

/** Function whose doc comment contains an unclosed tr tag.

    <table><tr></table>
 */
void f2();

/** Function whose doc comment contains an unclosed td tag.

    <table><tr><td></tr></table>
 */
void f3();

/** Function whose doc comment contains an unclosed thead tag.

    <table><thead></table>
 */
void f4();
