//
// Copyright (c) 2023 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include <mrdox/Support/Handlebars.hpp>
#include <mrdox/Support/Path.hpp>
#include <mrdox/Support/String.hpp>
#include <mrdox/Support/Dom.hpp>
#include "test_macros.hpp"
#include "diff.hpp"
#include <fmt/format.h>
#include <ranges>

void splitLines(std::string_view text, std::vector<std::string_view> &lines);

using namespace clang::mrdox;

template<>
class fmt::formatter<dom::Kind, char> {
public:
    template<typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(dom::Kind const& value, FormatContext& ctx) {
        switch (value) {
            case dom::Kind::Null:
                return format_to(ctx.out(), "null");
            case dom::Kind::Boolean:
                return format_to(ctx.out(), "boolean");
            case dom::Kind::Integer:
                return format_to(ctx.out(), "integer");
            case dom::Kind::String:
                return format_to(ctx.out(), "string");
            case dom::Kind::Array:
                return format_to(ctx.out(), "array");
            case dom::Kind::Object:
                return format_to(ctx.out(), "object");
        }
        return format_to(ctx.out(), "unknown");
    }
};


int
main() {
    /////////////////////////////////////////////////////////////////
    // Fixtures
    /////////////////////////////////////////////////////////////////
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

    /////////////////////////////////////////////////////////////////
    // Context
    /////////////////////////////////////////////////////////////////
    dom::Object context;
    dom::Object page;
    page.set("kind", "record");
    page.set("name", "from_chars");
    page.set("decl", "std::from_chars");
    page.set("loc", "charconv");
    dom::Object javadoc;
    javadoc.set("brief", "Converts strings to numbers");
    javadoc.set("details", "This function converts strings to numbers");
    page.set("javadoc", javadoc);
    page.set("synopsis", "This is the from_chars function");
    dom::Object person;
    person.set("firstname", "John");
    person.set("lastname", "Doe");
    page.set("person", person);
    dom::Array people = dom::newArray<dom::DefaultArrayImpl>();
    auto first_and_last_names = {
            std::make_pair("Alice", "Doe"),
            std::make_pair("Bob", "Doe"),
            std::make_pair("Carol", "Smith")};
    for (auto [firstname, lastname]: first_and_last_names) {
        person = {};
        person.set("firstname", firstname);
        person.set("lastname", lastname);
        dom::Array arr = dom::newArray<dom::DefaultArrayImpl>();
        arr.emplace_back(dom::Object{});
        arr.emplace_back(dom::Object{});
        arr.emplace_back(dom::Object{});
        arr.emplace_back(dom::Object{});
        person.set("book", arr);
        people.emplace_back(person);
    }
    page.set("people", people);
    page.set("prefix", "Hello");
    page.set("specialChars", "& < > \" ' ` =");
    page.set("url", "https://cppalliance.org/");
    dom::Object page_author;
    page_author.set("firstname", "Yehuda");
    page_author.set("lastname", "Katz");
    page.set("author", page_author);
    context.set("page", page);
    dom::Array nav = dom::newArray<dom::DefaultArrayImpl>();
    dom::Object nav1;
    nav1.set("url", "foo");
    nav1.set("test", true);
    nav1.set("title", "bar");
    nav.emplace_back(nav1);
    dom::Object nav2;
    nav2.set("url", "bar");
    nav.emplace_back(nav2);
    context.set("nav", nav);
    context.set("myVariable", "lookupMyPartial");
    dom::Object myOtherContext;
    myOtherContext.set("information", "Interesting!");
    context.set("myOtherContext", myOtherContext);
    context.set("favoriteNumber", 123);
    context.set("prefix", "Hello");
    context.set("title", "My Title");
    context.set("body", "My Body");
    dom::Object story;
    story.set("intro", "Before the jump");
    story.set("body", "After the jump");
    context.set("story", story);
    dom::Array comments = dom::newArray<dom::DefaultArrayImpl>();
    dom::Object comment1;
    comment1.set("subject", "subject 1");
    comment1.set("body", "body 1");
    comments.emplace_back(comment1);
    dom::Object comment2;
    comment2.set("subject", "subject 2");
    comment2.set("body", "body 2");
    comments.emplace_back(comment2);
    context.set("comments", comments);
    context.set("isActive", true);
    context.set("isInactive", false);
    dom::Object peopleObj;
    for (auto [firstname, lastname]: first_and_last_names) {
        person = {};
        person.set("firstname", firstname);
        person.set("lastname", lastname);
        peopleObj.set(firstname, person);
    }
    context.set("peopleobj", peopleObj);
    context.set("author", true);
    context.set("firstname", "Yehuda");
    context.set("lastname", "Katz");
    dom::Array names = dom::newArray<dom::DefaultArrayImpl>();
    names.emplace_back("Yehuda Katz");
    names.emplace_back("Alan Johnson");
    names.emplace_back("Charles Jolley");
    context.set("names", names);
    dom::Object namesobj;
    namesobj.set("Yehuda", "Yehuda Katz");
    namesobj.set("Alan", "Alan Johnson");
    namesobj.set("Charles", "Charles Jolley");
    context.set("namesobj", namesobj);
    dom::Object city;
    city.set("name", "San Francisco");
    city.set("summary", "San Francisco is the <b>cultural center</b> of <b>Northern California</b>");
    dom::Object location;
    location.set("north", "37.73,");
    location.set("east", "-122.44");
    city.set("location", location);
    city.set("population", 883305);
    context.set("city", city);

    dom::Object lookup_test;
    dom::Array people_lookup = dom::newArray<dom::DefaultArrayImpl>();
    people_lookup.emplace_back("Nils");
    people_lookup.emplace_back("Yehuda");
    lookup_test.set("people", people_lookup);
    dom::Array cities_lookup = dom::newArray<dom::DefaultArrayImpl>();
    cities_lookup.emplace_back("Darmstadt");
    cities_lookup.emplace_back("San Francisco");
    lookup_test.set("cities", cities_lookup);
    context.set("lookup_test", lookup_test);

    dom::Object lookup_test2;
    dom::Array persons = dom::newArray<dom::DefaultArrayImpl>();
    dom::Object person1;
    person1.set("name", "Nils");
    person1.set("resident-in", "darmstadt");
    persons.emplace_back(person1);
    dom::Object person2;
    person2.set("name", "Yehuda");
    person2.set("resident-in", "san-francisco");
    persons.emplace_back(person2);
    lookup_test2.set("persons", persons);
    dom::Object cities;
    dom::Object darmstadt;
    darmstadt.set("name", "Darmstadt");
    darmstadt.set("country", "Germany");
    cities.set("darmstadt", darmstadt);
    dom::Object san_francisco;
    san_francisco.set("name", "San Francisco");
    san_francisco.set("country", "USA");
    cities.set("san-francisco", san_francisco);
    lookup_test2.set("cities", cities);
    context.set("lookup_test2", lookup_test2);

    /////////////////////////////////////////////////////////////////
    // Register helpers
    /////////////////////////////////////////////////////////////////
    Handlebars hbs;
    helpers::registerAntoraHelpers(hbs);
    hbs.registerHelper("progress", [](dom::Array const& args) -> dom::Value {
        if (args.size() < 3) {
            return fmt::format("progress helper requires 3 arguments: {} provided", args.size());
        }
        if (!args[0].isString()) {
            return fmt::format("progress helper requires string argument: {} received", args[0]);
        }
        if (!args[1].isInteger()) {
            return fmt::format("progress helper requires number argument: {} received", args[1]);
        }
        if (!args[2].isBoolean()) {
            return fmt::format("progress helper requires boolean argument: {} received", args[2]);
        }
        dom::Value nameV = args[0];
        std::string_view name = nameV.getString();
        std::uint64_t percent = args[1].getInteger();
        bool stalled = args[2].getBool();
        std::uint64_t barWidth = percent / 5;
        std::string bar = std::string(20, '*').substr(0, barWidth);
        std::string stalledStr = stalled ? "stalled" : "";
        std::string res = fmt::format("{} {}% {} {}", bar, percent, name, stalledStr);
        return res;
    });

    hbs.registerHelper("noop", helpers::noop_fn);
    hbs.registerHelper("raw", helpers::noop_fn);

    hbs.registerHelper("link", [](dom::Array const& args, HandlebarsCallback const& cb) -> dom::Value {
        if (args.empty()) {
            return "no arguments provided to link helper";
        }
        for (std::size_t i = 1; i < args.size(); ++i) {
            if (!args[i].isString()) {
                return fmt::format("link helper requires string arguments: {} provided", args.size());
            }
        }

        std::string out;
        auto h = cb.hashes().find("href");
        if (h.isString()) {
            out += h.getString();
        } else if (args.size() > 1) {
            if (!args[1].isString()) {
                return fmt::format("link helper requires string argument: {} provided", args[1].kind());
            }
            auto href = args[1];
            out += href.getString();
        } else {
            out += "#";
        }

        out += '[';
        out += args[0].getString();
        // more attributes from hashes
        for (auto const& [key, value] : cb.hashes()) {
            if (key == "href" || !value.isString()) {
                continue;
            }
            out += ',';
            out += key;
            out += '=';
            out += value.getString();
        }
        out += ']';

        return out;
    });

    hbs.registerHelper("loud", [](
        dom::Array const& args,
        HandlebarsCallback const& cb) -> dom::Value {
        std::string res;
        if (cb.isBlock()) {
            res = cb.fn();
        } else {
            if (args.empty()) {
                return "loud helper requires at least one argument";
            }
            if (!args[0].isString()) {
                return fmt::format("loud helper requires string argument: {} provided", args[0].kind());
            }
            res = args[0].getString();
        }
        for (char& c : res) {
            if (c >= 'a' && c <= 'z')
                c += 'A' - 'a';
        }
        return res;
    });

    hbs.registerHelper("to_string", [](
        dom::Array const& args) -> dom::Value {
        if (args.empty()) {
            return "to_string helper requires at least one argument";
        }
        dom::Value arg = args[0];
        return JSON_stringify(arg);
    });

    hbs.registerHelper("bold", [](
        dom::Array const& /* args */,
        HandlebarsCallback const& cb) -> dom::Value {
        return fmt::format(R"(<div class="mybold">{}</div>)", cb.fn());
    });

    hbs.registerHelper("list", [](
        dom::Array const& args,
        HandlebarsCallback const& cb) -> dom::Value {
        // Built-in helper to change the context for each object in args
        if (args.size() != 1) {
            return fmt::format("list helper requires 1 argument: {} provided", args.size());
        }
        if (!args[0].isArray()) {
            return fmt::format("list helper requires array argument: {} provided", args[0].kind());
        }
        dom::Value itemsV = args[0];
        dom::Array items = itemsV.getArray();
        if (!items.empty()) {
            std::string out = "<ul";
            for (auto const& [key, value] : cb.hashes()) {
                out += " ";
                out += key;
                out += "=\"";
                out += value.getString();
                out += "\"";
            }
            out += ">";
            for (std::size_t i = 0; i < items.size(); ++i) {
                dom::Value item = items[i];
                // AFREITAS: this logic should be in private data
                dom::Object frame = item.getObject();
                frame.set("..", cb.context());
                frame.set("@key", i);
                frame.set("@first", i == 0);
                frame.set("@last", i == items.size() - 1);
                frame.set("@index", i);
                out += "<li>" + cb.fn(frame) + "</li>";
            }
            return out + "</ul>";
        }
        return cb.inverse();
    });

    hbs.registerHelper("isdefined", [](dom::Array const& args) -> dom::Value {
        if (args.empty()) {
            return "isdefined helper requires at least one argument";
        }
        // There's no distinction between null and undefined in mrdox::dom
        return !args[0].isNull();
    });

    hbs.registerHelper("helperMissing", [](
        dom::Array const& args,
        HandlebarsCallback const& cb) -> dom::Value {
        std::string out;
        OutputRef os(out);
        os << "Missing: ";
        os << cb.name;
        os << "(";
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i != 0) {
                os << ", ";
            }
            os << args[i];
        }
        os << ")";
        return out;
    });

    hbs.registerHelper("blockHelperMissing", [](
        dom::Array const& args,
        HandlebarsCallback const& cb) -> dom::Value {
        std::string out;
        OutputRef os(out);
        os << "Helper '";
        os << cb.name;
        os << "' not found. Printing block: ";
        os << cb.fn();
        return out;
    });

    /////////////////////////////////////////////////////////////////
    // Register partials
    /////////////////////////////////////////////////////////////////
    // From files
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

    // Dynamic partial helpers
    hbs.registerHelper("whichPartial", []() -> dom::Value {
        return "dynamicPartial";
    });

    // Literal partials
    hbs.registerPartial("dynamicPartial", "Dynamo!");
    hbs.registerPartial("lookupMyPartial", "Found!");
    hbs.registerPartial("myPartialContext", "{{information}}");
    hbs.registerPartial("myPartialParam", "The result is {{parameter}}");
    hbs.registerPartial("myPartialParam2", "{{prefix}}, {{firstname}} {{lastname}}");
    hbs.registerPartial("layoutTemplate", "Site Content {{> @partial-block }}");
    hbs.registerPartial("pageLayout", "<div class=\"nav\">\n  {{> nav}}\n</div>\n<div class=\"content\">\n  {{> content}}\n</div>");

    /////////////////////////////////////////////////////////////////
    // Render and diff
    /////////////////////////////////////////////////////////////////
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

    fmt::println("All tests passed!");
    return EXIT_SUCCESS;
}
