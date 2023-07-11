//
// Copyright (c) 2023 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#ifndef MRDOX_TEST_DIFF_HPP
#define MRDOX_TEST_DIFF_HPP

#include <string>
#include <string_view>

namespace test_suite
{

/** Result of a diff between two strings
 */
struct DiffStringsResult
{
    /** The diff between the two strings
     *
     * The diff is a string that contains the differences between the two
     * strings.
     *
     * New lines are prefixed with '+' and removed lines are prefixed with '-'.
     */
    std::string diff;

    /// The number of lines added in the contents
    int added{0};

    /// The number of lines removed in the contents
    int removed{0};

    /// The number of unmodified lines in the contents
    int unmodified{0};
};

/** Perform a diff between two strings and check if they are equal

    This function is used to compare the contents of a file with the expected
    contents of a file. If they are different, the diff is printed to the
    console and the test fails.

    The procedure assumes the expected_contents are never empty.
    If the expected_contents is empty, the rendered_contents is always
    considered valid, the test passes and the rendered contents are
    considered the expected contents, so it's saved to the expected
    contents path for the next execution.

    If the expected_contents is not empty, the rendered_contents is
    compared to the expected contents with the LCS algorithm.
    If the rendered_contents is different from the expected contents,
    the difference between the contents is printed to the console and
    the test fails.

    @param expected_contents The expected contents of the file
    @param expected_contents_path The path to the expected contents file
    @param rendered_contents The rendered contents of the file
    @param error_output_path The path to the error output file
 */
void
BOOST_TEST_DIFF(
    std::string_view expected_contents,
    std::string_view expected_contents_path,
    std::string_view rendered_contents,
    std::string_view error_output_path);

}

#endif //MRDOX_TEST_DIFF_HPP
