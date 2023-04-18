// Header file for testing a large number
// of threads processing files including
// the same header

#ifndef LARGE_HPP
#define LARGE_HPP

#include <string>

/** This is one class.

    This class is designed to exercise all of the
    doc comment extraction features of MrDox.

    This sentence starts a new @b paragraph.
    This is the second sentence of that paragraph.

    @code
    // example code
    int main()
    {
        return 0;
    }
    @endcode

    Another block of text.
*/
class OneClass
{
public:
    /** This is T1
    */
    using T1 = int;

    /** This is T2
    */
    using T2 = char;

    /** And this is a std type
    */
    typedef std::string StringType;

    /** Destructor.
    */
    ~OneClass();

    /** Constructor.
    */
    OneClass();

    /** Constructor.

        @param x Any number
    */
    OneClass(int x);

    /** Constructor.
    */
    OneClass(OneClass const&) = default;

    /** Assignment
    */
    OneClass& operator=(OneClass const&) = default;

protected:
    /** An implementation detail for subclasses.
    */
    int implementationDetail();
};

#endif
