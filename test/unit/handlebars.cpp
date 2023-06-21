//
// Copyright (c) 2023 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include <mrdox/Support/Handlebars.hpp>
#include <mrdox/Support/Path.hpp>
#include <mrdox/Support/String.hpp>
#include "test_macros.hpp"
#include <fmt/format.h>
#include <ranges>

void splitLines(std::string_view text, std::vector<std::string_view> &lines);

using namespace clang::mrdox;

template<>
class fmt::formatter<llvm::json::Value::Kind, char> {
public:
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(llvm::json::Value::Kind const& value, FormatContext& ctx) {
        switch (value) {
            case llvm::json::Value::Kind::Null:
                return format_to(ctx.out(), "null");
            case llvm::json::Value::Kind::Boolean:
                return format_to(ctx.out(), "boolean");
            case llvm::json::Value::Kind::Number:
                return format_to(ctx.out(), "number");
            case llvm::json::Value::Kind::String:
                return format_to(ctx.out(), "string");
            case llvm::json::Value::Kind::Array:
                return format_to(ctx.out(), "array");
            case llvm::json::Value::Kind::Object:
                return format_to(ctx.out(), "object");
        }
        return format_to(ctx.out(), "unknown");
    }
};

template<>
class fmt::formatter<llvm::json::Value, char> {
public:
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(llvm::json::Value const& value, FormatContext& ctx) {
        switch (value.kind()) {
            case llvm::json::Value::Kind::Null:
                return format_to(ctx.out(), "null");
            case llvm::json::Value::Kind::Boolean:
                return format_to(ctx.out(), "{}", value.getAsBoolean().value());
            case llvm::json::Value::Kind::Number:
                return format_to(ctx.out(), "{}", value.getAsNumber().value());
            case llvm::json::Value::Kind::String:
                return format_to(ctx.out(), "{}", std::string_view(value.getAsString().value()));
            case llvm::json::Value::Kind::Array:
                return format_to(ctx.out(), "[array]");
            case llvm::json::Value::Kind::Object:
                return format_to(ctx.out(), "[object]");
        }
        return format_to(ctx.out(), "unknown");
    }
};

namespace {
    // Helpers from the handlebars documentation
    llvm::json::Value
    loud_fn(std::string_view arg) {
        return llvm::StringRef(arg).upper();
    }

    llvm::json::Value
    bold_fn(
        llvm::json::Object const& ctx,
        llvm::json::Array const& /* args */,
        HandlebarsCallback const& cb) {
        return fmt::format(R"(<div class="mybold">{}</div>)", cb.fn(ctx));
    }

    llvm::json::Value
    link_fn(
            llvm::json::Array const& args,
            HandlebarsCallback const& cb) {
        if (args.empty()) {
            return "no arguments provided to link helper";
        }
        if (!std::ranges::all_of(args, [](const auto& v){ return v.kind() == llvm::json::Value::Kind::String; })) {
            return "link helper requires string arguments";
        }
        std::string out;
        auto it = cb.hashes.find("href");
        if (it != cb.hashes.end()) {
            out += it->second.getAsString().value();
        } else if (args.size() > 1) {
            out += args[1].getAsString().value();
        } else {
            out += "#";
        }
        out += '[';
        out += args[0].getAsString().value();
        // more attributes from hashes
        for (auto const& [key, value] : cb.hashes) {
            if (key == "href" || value.kind() != llvm::json::Value::Kind::String) {
                continue;
            }
            out += ',';
            out += llvm::StringRef(key);
            out += '=';
            out += value.getAsString().value();
        }
        out += ']';
        return out;
    }

    llvm::json::Value
    progress_fn(llvm::json::Array const& args) {
        // All the validation below could be provided by the registerHelper
        // overloads once we better generalize it.
        if (args.size() < 3) {
            return fmt::format("progress helper requires 3 arguments: {} provided", args.size());
        }
        if (args[0].kind() != llvm::json::Value::Kind::String) {
            return fmt::format("progress helper requires string argument: {} received", args[0]);
        }
        if (args[1].kind() != llvm::json::Value::Kind::Number) {
            return fmt::format("progress helper requires number argument: {} received", args[1]);
        }
        if (args[2].kind() != llvm::json::Value::Kind::Boolean) {
            return fmt::format("progress helper requires boolean argument: {} received", args[2]);
        }
        std::string_view name = args[0].getAsString().value();
        std::uint64_t percent = args[1].getAsUINT64().value();
        bool stalled = args[2].getAsBoolean().value();
        std::uint64_t barWidth = percent / 5;
        std::string bar = std::string(20, '*').substr(0, barWidth);
        std::string stalledStr = stalled ? "stalled" : "";
        return fmt::format("{} {}% {} {}", bar, percent, name, stalledStr);
    }

    llvm::json::Value
    to_string_fn(
            llvm::json::Value const& arg) {
        std::string out;
        llvm::raw_string_ostream stream(out);
        stream << arg;
        return out;
    }

    llvm::json::Value
    list_fn(
    llvm::json::Object const& context,
    llvm::json::Array const& args,
    HandlebarsCallback const& cb) {
        // Built-in helper to change the context for each object in args
        if (!args.empty()) {
            std::string out = "<ul>";
            for (auto const& arg : args) {
                if (arg.kind() == llvm::json::Value::Kind::Object) {
                    llvm::json::Object frame = *arg.getAsObject();
                    frame.try_emplace("..", llvm::json::Object(context));
                    out += "<li>" + cb.fn(frame) + "</li>";
                }
            }
            return out + "</ul>";
        }
        return cb.inverse(context);
    }

}

struct DiffStringsResult {
    std::string diff;
    int added{0};
    int removed{0};
    int unchanged{0};
};

std::string_view
trim_spaces(std::string_view expression)
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
}


// Diff two strings and return the result as a string with additional stats
DiffStringsResult
diffStrings(std::string_view str1, std::string_view str2, std::size_t context_size = 3) {
    std::vector<std::string_view> lines1;
    splitLines(str1, lines1);
    std::vector<std::string_view> lines2;
    splitLines(str2, lines2);

    // Initialize the Longest Common Subsequence (LCS) table
    // A Longest Common Subsequence (LCS) is a sequence that appears
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
    for (size_t i = 0; i < lines1.size(); ++i) {
        for (size_t j = 0; j < lines2.size(); ++j) {
            if (trim_spaces(lines1[i]) == trim_spaces(lines2[j])) {
                // If the lines are equal, it means they contribute to the common subsequence.
                // In this case, the value in the current cell lcsTable[i + 1][j + 1] is set
                // to the value in the diagonal cell lcsTable[i][j] incremented by 1
                lcsTable[i + 1][j + 1] = lcsTable[i][j] + 1;
            } else {
                // If the lines are not equal, the algorithm takes the maximum value between
                // the cell on the left (lcsTable[i + 1][j]) and the cell above (lcsTable[i][j + 1])
                // and stores it in the current cell lcsTable[i + 1][j + 1].
                // This step ensures that the table holds the length of the longest common
                // subsequence found so far.
                lcsTable[i + 1][j + 1] = std::max(lcsTable[i + 1][j], lcsTable[i][j + 1]);
            }
        }
    }

    // Traceback to find the differences
    DiffStringsResult result;
    struct DiffLineResult {
        std::string line;
        bool added{false};
        bool removed{false};
    };
    std::vector<DiffLineResult> diffLines;
    size_t i = lines1.size();
    size_t j = lines2.size();

    // Traverse the LCS table
    // The algorithm starts in the bottom right corner of the table
    // Starting from the bottom-right cell of the LCS table, it examines
    // the adjacent cells to determine the direction of the LCS
    while (i > 0 && j > 0) {
        if (lines1[i - 1] == lines2[j - 1]) {
            // If the current lines lines1[i-1] and lines2[j-1] are equal,
            // it means the line is common to both multiline strings. It
            // is added to diffLines with a space prefix, indicating no change.
            diffLines.push_back({std::string(lines1[i - 1]), false, false});
            --i;
            --j;
            result.unchanged++;
        } else if (lcsTable[i][j - 1] >= lcsTable[i - 1][j]) {
            // If the value in the cell on the left, lcsTable[i][j-1], is
            // greater than or equal to the value in the cell above,
            // lcsTable[i-1][j], it means the line in lines2[j-1] is
            // part of the LCS. Thus, it is added to diffLines with
            // a "+" prefix to indicate an addition.
            diffLines.push_back({std::string(lines2[j - 1]), true, false});
            --j;
            result.added++;
        } else {
            // Otherwise, the line in lines1[i-1] is part of the LCS, and it
            // is added to diffLines with a "-" prefix to indicate a deletion.
            diffLines.push_back({std::string(lines1[i - 1]), false, true});
            --i;
            result.removed++;
        }
    }

    while (i > 0) {
        diffLines.push_back({std::string(lines1[i - 1]), false, true});
        --i;
        result.removed++;
    }

    while (j > 0) {
        diffLines.push_back({std::string(lines2[j - 1]), true, false});
        --j;
        result.added++;
    }

    // Reverse the diff lines to match the original order
    std::reverse(diffLines.begin(), diffLines.end());

    // Concatenate diff lines into a single string considering number of unchanged lines in the context
    std::size_t unchanged = 0;
    std::size_t last_rendered = std::size_t(-1);
    for (i = 0; i < diffLines.size(); ++i) {
        auto& diffLine = diffLines[i];
        if (diffLine.added || diffLine.removed) {
            std::size_t context_begin = std::max(i - context_size, (last_rendered == std::size_t(-1) ? 0 : last_rendered + 1));
            std::size_t out_of_context = unchanged - (i - context_begin);
            if (out_of_context > 0) {
                result.diff += fmt::format("... {} unchanged line(s)\n", out_of_context);
                unchanged = 0;
            }
            for (j = context_begin; j < i; ++j) {
                result.diff += fmt::format("{}\n", diffLines[j].line);
            }
            result.diff += fmt::format("{} {}\n", diffLine.added ? '+' : '-', diffLine.line.empty() ? "     (empty line)" : diffLine.line);
            std::size_t next_changed = i + 1;
            while (
                next_changed < diffLines.size() &&
                !(diffLines[next_changed].added || diffLines[next_changed].removed) &&
                next_changed - i < context_size) {
                next_changed++;
            }
            std::size_t context_end = std::min(i + context_size + 1, next_changed);
            for (j = i + 1; j < context_end; ++j) {
                result.diff += fmt::format("{}\n", diffLines[j].line);
            }
            // not really changed but rendered
            last_rendered = context_end - 1;
        } else {
            unchanged++;
        }
    }
    if (unchanged <= context_size) {
        for (i = diffLines.size() - context_size; i < diffLines.size(); ++i) {
            result.diff += fmt::format("{}\n", diffLines[i].line);
        }
    } else {
        result.diff += fmt::format("... {} unchanged line(s)\n", unchanged);
    }

    return result;
}

void splitLines(std::string_view text, std::vector<std::string_view> &lines) {
    size_t pos = 0;
    while (pos < text.length()) {
        size_t newPos = text.find('\n', pos);
        if (newPos == std::string::npos) {
            newPos = text.length();
        }
        lines.push_back(text.substr(pos, newPos - pos));
        pos = newPos + 1;
    }
}

int
main() {
    std::string_view template_path = MRDOX_UNIT_TEST_DIR "/fixtures/handlebars_features_test.adoc.hbs";
    std::string_view partial_paths[] = {
        MRDOX_UNIT_TEST_DIR "/fixtures/record-detail.adoc.hbs",
        MRDOX_UNIT_TEST_DIR "/fixtures/escaped.adoc.hbs"};
    std::string_view output_path = MRDOX_UNIT_TEST_DIR "/fixtures/handlebars_features_test.adoc";
    std::string_view error_output_path = MRDOX_UNIT_TEST_DIR "/fixtures/handlebars_features_test_error.adoc";
    auto template_text_r = files::getFileText(template_path);
    REQUIRE(template_text_r);
    auto master_file_contents_r = files::getFileText(output_path);
    auto template_str = *template_text_r;
    REQUIRE_FALSE(template_str.empty());

    HandlebarsOptions options;
    options.noHTMLEscape = true;

    // Create context for tests
    llvm::json::Object context;
    llvm::json::Object page;
    page["kind"] = "record";
    page["name"] = "from_chars";
    page["decl"] = "std::from_chars";
    page["loc"] = "charconv";
    llvm::json::Object javadoc;
    javadoc["brief"] = "Converts strings to numbers";
    javadoc["details"] = "This function converts strings to numbers";
    page["javadoc"] = std::move(javadoc);
    page["synopsis"] = "This is the from_chars function";
    llvm::json::Object person;
    person["firstname"] = "John";
    person["lastname"] = "Doe";
    page["person"] = std::move(person);
    llvm::json::Array people;
    auto first_and_last_names = {
            std::make_pair("Alice", "Doe"),
            std::make_pair("Bob", "Doe"),
            std::make_pair("Carol", "Smith")};
    for (auto [firstname, lastname]: first_and_last_names) {
        person = {};
        person["firstname"] = firstname;
        person["lastname"] = lastname;
        people.push_back(std::move(person));
    }
    page["people"] = std::move(people);
    page["prefix"] = "Hello";
    page["specialChars"] = "& < > \" ' ` =";
    page["url"] = "https://cppalliance.org/";
    context["page"] = std::move(page);
    context["nav"] = llvm::json::Array{
            llvm::json::Object{
                    {"url", "foo"},
                    {"test", true},
                    {"title", "bar"}},
            llvm::json::Object{
                    {"url", "bar"}}};
    context["myVariable"] = "lookupMyPartial";
    context["myOtherContext"] = llvm::json::Object{};
    context["myOtherContext"].getAsObject()->operator[]("information") = "Interesting!";
    context["favoriteNumber"] = 123;
    context["prefix"] = "Hello";
    context["title"] = "My Title";
    context["body"] = "My Body";
    llvm::json::Object story;
    story["intro"] = "Before the jump";
    story["body"] = "After the jump";
    context["story"] = std::move(story);
    llvm::json::Array comments;
    comments.push_back(llvm::json::Object{
            {"subject", "subject 1"},
            {"body", "body 1"}});
    comments.push_back(llvm::json::Object{
            {"subject", "subject 2"},
            {"body", "body 2"}});
    context["comments"] = std::move(comments);
    context["isActive"] = true;


    // Register some extra test helpers
    Handlebars hbs;
    helpers::registerStandardHelpers(hbs);
    helpers::registerAntoraHelpers(hbs);
    hbs.registerHelper("progress", progress_fn);
    hbs.registerHelper("link", link_fn);
    hbs.registerHelper("loud", loud_fn);
    hbs.registerHelper("to_string", to_string_fn);
    hbs.registerHelper("bold", bold_fn);
    hbs.registerHelper("list", list_fn);

    for (auto partial_path: partial_paths) {
        auto partial_text_r = files::getFileText(partial_path);
        REQUIRE(partial_text_r);
        std::string_view filename = files::getFileName(partial_path);
        auto pos = filename.find('.');
        if (pos != std::string_view::npos) {
            filename = filename.substr(0, pos);
        }
        hbs.registerPartial(filename, *partial_text_r);
    }

    hbs.registerHelper("whichPartial", [](
            llvm::json::Object const& /* context */,
            llvm::json::Array const& /* args */,
            HandlebarsCallback const& /* callback params */) -> llvm::json::Value {
        return "dynamicPartial";
    });
    hbs.registerPartial("dynamicPartial", "Dynamo!");
    hbs.registerPartial("lookupMyPartial", "Found!");
    hbs.registerPartial("myPartialContext", "{{information}}");
    hbs.registerPartial("myPartialParam", "The result is {{parameter}}");
    hbs.registerPartial("myPartialParam2", "{{prefix}}, {{firstname}} {{lastname}}");
    hbs.registerPartial("layoutTemplate", "Site Content {{> @partial-block }}");
    hbs.registerPartial("pageLayout", "<div class=\"nav\">\n  {{> nav}}\n</div>\n<div class=\"content\">\n  {{> content}}\n</div>");

    // Render template with all handlebars features
    std::string rendered_text = hbs.render(template_str, context, options);
    REQUIRE_FALSE(rendered_text.empty());

    // Compare template with reference
    if (!master_file_contents_r || master_file_contents_r->empty()) {
        // Write rendered template to file with ofstream
        std::ofstream out((std::string(output_path)));
        REQUIRE(out);
        fmt::println("Parsed template:\n{}", rendered_text);
        out << rendered_text;
    } else {
        // Compare rendered template with reference
        auto master_file_contents = *master_file_contents_r;
        DiffStringsResult diff = diffStrings(master_file_contents, rendered_text);
        if (diff.added > 0 || diff.removed > 0) {
            std::ofstream out((std::string(error_output_path)));
            REQUIRE(out);
            out << rendered_text;

            fmt::println("DIFF:\n=====================\n{}\n=====================", diff.diff);
            REQUIRE(diff.added == 0);
            REQUIRE(diff.removed == 0);
        }
        REQUIRE(rendered_text.size() == master_file_contents.size());
        REQUIRE(rendered_text == master_file_contents);
    }

    // Continue from:
    // https://handlebarsjs.com/guide/expressions.html#whitespace-control
    fmt::println("All tests passed!");
    return EXIT_SUCCESS;
}
