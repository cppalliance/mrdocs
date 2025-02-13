/*
    Copy details has special behavior for
    documentation detail blocks and for metadata.

    Unlike copybrief, the details should go
    exactly where the copydetails command is
    found.

    In terms of metadata (returns, params, etc),
    only non-ambiguous metadata should be copied.

    In the general case, only metadata that not
    already present in the destination should be
    copied. If the destination already provides
    preconditions, postconditions, sees, and
    returns documentation, the copydetails command
    should be ignored for these fields.

    In the case of parameters and template parameters,
    only those that are not already present in the
    destination AND exist in the destination symbol
    declaration should be copied.
 */

/** Source doc function

    This is the documentation from the
    source function.

    @pre a must be greater than 0.
    @post The return value is greater than 0.

    @throws std::runtime_error If something goes wrong.

    @tparam A The template parameter A.
    @tparam B The template parameter B.
    @param a The parameter a.
    @param b The parameter b.
    @return A nice integer

    @see dest
 */
template <class A, class B>
int
source(int a, int b);

/** Destination doc function

    Documentation prefix for dest only.

    @copydetails source

    Documentation suffix for dest only.
 */
template <class A, class B>
int
dest(int a, int b);

/** Destination doc function

    Documentation prefix for dest only.

    @copydetails source

    Documentation suffix for dest only.

    Parameter b is not copied because
    it doesn't exist in the destination function.

    @pre overriden precondition
    @post overriden postcondition

    @throws std::runtime_error Overwrites the exception.

    @tparam A Overwrites the template parameter A.
    @tparam C The template parameter C.
    @tparam D The template parameter D.
    @param a Overwrites the parameter a.
    @param c The parameter c.
    @param d The parameter d.
    @return An integer meaning something else.

    @see source
 */
template <class A, class C, class D>
int
destOverride(int a, int c, int d);
