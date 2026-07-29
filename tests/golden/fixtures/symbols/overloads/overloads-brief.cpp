/** Function with same brief

    @param a Describe a
 */
void sameBrief(int a);

/** Function with same brief

    @param a Describe a again
    @param b Describe b
 */
void sameBrief(int a, int b);

/// Overload briefs from function or operator classes
struct A {
    /** First constructor
     */
    A();

    /** Second constructor

        @param a Describe a
     */
    A(int a);

    /** Assign from A

        @param a Describe a
        @return `*this`
     */
    A&
    operator=(A const& a);

    /** Assign from int

        @param a Describe b
        @return `*this`
     */
    A&
    operator=(int a);

    /** Addition operator for As

        @param a Describe a
        @return `*this`
     */
    A operator+(A const& a);

    /** Addition operator for ints

        @param a Describe a
        @return `*this`
     */
    A operator+(int a);

    /** Unary operator- for A

        No way to generate a brief from the
        operator kind because there are
        unary and binary operators.

        @return Result
     */
    A operator-();

    /** Binary operator- for A

        No way to generate a brief from the
        operator kind.

        @return Result
     */
    A operator-(A const&);
};

/// Auxiliary class
struct B{};

/** Unary operator for A

    @return Result
*/
int operator+(A const&);

/** Unary operator for B

    @return Result
*/
int operator+(B const&);

/** Function with no params
 */
void no_way_to_infer_this_brief();

/** Function with single param

    @param a Describe a
 */
void no_way_to_infer_this_brief(int a);