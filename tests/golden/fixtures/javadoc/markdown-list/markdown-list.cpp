/** A simple list with Markdown bold.

    Here's the list:
    - **Bold text**: Description 1.
    - **Bold text**: Description 2.
*/
class A;

/** A list with inline code and list items containing "- ".

    A sentence with `inline code`. Now a list:
    - `foo()` - A dummy function.
    - `bar()` / `baz()` - Other two functions.
    - `qux()` - Yet another function.
    - `quux()` - Guess what?
*/
class B;

/** A list with no preceding text.

    - First item.
    - Second item.
*/
class C;

/** A list with Markdown bold and code.

    This is a list:
    - **Bold** blah blah blah.
    - **Bold** and `code`.
*/
void f();

/** A list with Markdown code and a nested list.

   @par This is a paragraph
   @li `code` followed by normal text:
       - This is a `nested` list with `code`.
       - This `is` another list item.
*/
void g();

/** A function with a list inside a note.

    @note Important points:
    - First point.
    - Second point.
*/
void h();

/** Marker with styled continuation only.

    - **All bold item.**
    - `All code item.`
*/
void i();

/** Brief text.

    - @c foo does something.
    - @c bar does something else.
*/
void j();
