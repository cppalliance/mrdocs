//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/url
//

#include "diff.hpp"
#include "test_suite.hpp"
#include <algorithm>
#include <format>
#include <fstream>
#include <print>
#include <vector>

namespace test_suite {
// Diff two strings and return the result as a string with additional stats
DiffStringsResult
diffStrings(std::string_view str1, std::string_view str2, std::size_t context_size)
{
    static constexpr auto splitLines =
        [](std::string_view text, std::vector<std::string_view> &lines)
    {
        size_t pos = 0;
        while (pos < text.length())
        {
            size_t newPos = text.find('\n', pos);
            if (newPos == std::string::npos)
            {
                newPos = text.length();
            }
            auto line = text.substr(pos, newPos - pos);
            lines.push_back(line);
            pos = newPos + 1;
        }
    };

    static constexpr auto trim_spaces =
        [](std::string_view expression) -> std::string_view

    {
            auto pos = expression.find_first_not_of(" \t\r\n");
            if (pos == std::string_view::npos)
            return "";
        expression.remove_prefix(pos);
            pos = expression.find_last_not_of(" \t\r\n");
            if (pos == std::string_view::npos)
            return "";
        expression.remove_suffix(expression.size() - pos - 1);
            return expression;
    };

    std::vector<std::string_view> lines1;
    splitLines(str1, lines1);
    std::vector<std::string_view> lines2;
    splitLines(str2, lines2);

    // Initialize the Longest Common Subsequence (LCS) table
    // The Longest Common Subsequence (LCS) is a sequence that appears
    // as a subsequence in two or more given sequences.
    // In diff, the LCS refers to the longest sequence of lines that are
    // common between two multiline strings.
    // It has dimensions (lines1.size() + 1) x (lines2.size() + 1), where lines1.size() and lines2.size()
    // represent the lengths of lines1 and lines2 respectively.
    // Each cell of the table holds the length of the LCS for the
    // corresponding prefixes of lines1 and lines2
    // The initialization sets all values in the table to 0,
    // indicating that there is no common subsequence found yet
    std::vector<std::vector<size_t>> lcsTable(
            lines1.size() + 1,
            std::vector<size_t>(
                    lines2.size() + 1, 0));

    // Build the LCS table
    // This code builds the LCS table by iteratively comparing each
    // line of lines1 with each line of lines2.
    // The LCS algorithm populates the table based on the lengths
    // of the longest common subsequences found so far
    for (size_t i = 0; i < lines1.size(); ++i)
    {
        for (size_t j = 0; j < lines2.size(); ++j)
        {
            auto line1 = trim_spaces(lines1[i]);
            auto line2 = trim_spaces(lines2[j]);
            if (line1 == line2)
            {
                // If the lines are equal, it means they contribute to the common subsequence.
                // In this case, the value in the current cell lcsTable[i + 1][j + 1] is set
                // to the value in the diagonal cell lcsTable[i][j] incremented by 1
                lcsTable[i + 1][j + 1] = lcsTable[i][j] + 1;
            }
            else
            {
                // If the lines are not equal, the algorithm takes the maximum value between
                // the cell on the left (lcsTable[i + 1][j]) and the cell above (lcsTable[i][j + 1])
                // and stores it in the current cell lcsTable[i + 1][j + 1].
                // This step ensures that the table holds the length of the longest common
                // subsequence found so far.
                lcsTable[i + 1][j + 1] = (std::max)(lcsTable[i + 1][j], lcsTable[i][j + 1]);
            }
        }
    }

    // Traceback to find the differences
    DiffStringsResult result;
    struct DiffLineResult
    {
        std::string line;
        bool added{false};
        bool removed{false};
        bool in_context = false;
    };
    std::vector<DiffLineResult> diffLines;
    size_t i = lines1.size();
    size_t j = lines2.size();

    // Traverse the LCS table
    // The algorithm starts in the bottom right corner of the table
    // Starting from the bottom-right cell of the LCS table, it examines
    // the adjacent cells to determine the direction of the LCS
    while (i > 0 && j > 0)
    {
        auto line1 = trim_spaces(lines1[i - 1]);
        auto line2 = trim_spaces(lines2[j - 1]);
        if (line1 == line2)
        {
            // If the current lines lines1[i-1] and lines2[j-1] are equal,
            // it means the line is common to both multiline strings. It
            // is added to diffLines with a space prefix, indicating no change.
            diffLines.push_back({std::string(line1), false, false});
            --i;
            --j;
            result.unmodified++;
        }
        else if (lcsTable[i][j - 1] >= lcsTable[i - 1][j])
        {
            // If the value in the cell on the left, lcsTable[i][j-1], is
            // greater than or equal to the value in the cell above,
            // lcsTable[i-1][j], it means the line in lines2[j-1] is
            // part of the LCS. Thus, it is added to diffLines with
            // a "+" prefix to indicate an addition.
            diffLines.push_back({std::string(line2), true, false});
            --j;
            result.added++;
        }
        else
        {
            // Otherwise, the line in lines1[i-1] is part of the LCS, and it
            // is added to diffLines with a "-" prefix to indicate a deletion.
            diffLines.push_back({std::string(line1), false, true});
            --i;
            result.removed++;
        }
    }

    while (i > 0)
    {
        auto line1 = lines1[i - 1];
        diffLines.push_back({std::string(line1), false, true});
        --i;
        result.removed++;
    }

    while (j > 0)
    {
        auto line2 = lines2[j - 1];
        diffLines.push_back({std::string(line2), true, false});
        --j;
        result.added++;
    }

    // Reverse the diff lines to match the original order
    std::reverse(diffLines.begin(), diffLines.end());

    // Mark diffLines in context
    std::vector<size_t> modifiedIndexes;
    for (i = 0; i < diffLines.size(); ++i)
    {
        auto& diffLine = diffLines[i];
        if (diffLine.added || diffLine.removed)
        {
           modifiedIndexes.push_back(i);
        }
    }

    // Mark diffLines in context
    for (i = 0; i < modifiedIndexes.size(); ++i)
    {
        auto& diffLine = diffLines[modifiedIndexes[i]];
        diffLine.in_context = true;
        for (j = 1; j <= context_size; ++j)
        {
            if (modifiedIndexes[i] >= j)
            {
                diffLines[modifiedIndexes[i] - j].in_context = true;
            }
            if (modifiedIndexes[i] + j < diffLines.size())
            {
                diffLines[modifiedIndexes[i] + j].in_context = true;
            }
        }
    }

    // Concatenate diff lines into a single string considering the number
    // of unmodified lines in the context
    std::size_t out_of_context = 0;
    for (auto diffLine : diffLines)
    {
        if (!diffLine.in_context)
        {
            out_of_context++;
            continue;
        }
        if (out_of_context > 0)
        {
          result.diff +=
              std::format("... {} unmodified line(s)\n", out_of_context);
          out_of_context = 0;
        }
        if (diffLine.added || diffLine.removed)
        {
          result.diff += std::format("{} {}\n", diffLine.added ? '+' : '-',
                                     diffLine.line.empty() ? "     (empty line)"
                                                           : diffLine.line);
        }
        else
        {
          result.diff += std::format("{}\n", diffLine.line);
        }
    }
    if (out_of_context > 0)
    {
      result.diff += std::format("... {} unmodified line(s)", out_of_context);
    }

    return result;
}

void
BOOST_TEST_DIFF(
    std::string_view expected_contents,
    std::string_view expected_contents_path,
    std::string_view rendered_contents,
    std::string_view error_output_path)
{
    // Compare template with reference
    if (expected_contents.empty())
    {
        // Write rendered template to file with ofstream
        std::ofstream out((std::string(expected_contents_path)));
        BOOST_TEST(out);
        std::println("Parsed template:\n{}", rendered_contents);
        out << rendered_contents;
    }
    else
    {
        // Compare rendered template with reference
        DiffStringsResult diff = diffStrings(expected_contents, rendered_contents);
        if (diff.added > 0 || diff.removed > 0)
        {
            if (!error_output_path.empty())
            {
                std::ofstream out((std::string(error_output_path)));
                BOOST_TEST(out);
                out << rendered_contents;
            }
            std::println(
                "DIFF:\n=====================\n{}\n=====================",
                diff.diff);
            BOOST_TEST(diff.added == 0);
            BOOST_TEST(diff.removed == 0);
        }
    }
}

} // test_suite
