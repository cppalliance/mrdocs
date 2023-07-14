//
// Copyright (c) 2023 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include <test_suite/detail/decomposer.hpp>
#include <test_suite/diff.hpp>
#include <test_suite/test_suite.hpp>
#include <ranges>
#include <fmt/format.h>
#include <mrdox/Support/Dom.hpp>
#include <mrdox/Support/Handlebars.hpp>
#include <mrdox/Support/Path.hpp>
#include <mrdox/Support/String.hpp>

namespace clang {
namespace mrdox {

struct Handlebars_test
{

static constexpr
std::string_view
to_string(clang::mrdox::dom::Kind const& value)
{
    switch (value)
    {
        case clang::mrdox::dom::Kind::Null:
            return "null";
        case clang::mrdox::dom::Kind::Boolean:
            return "boolean";
        case clang::mrdox::dom::Kind::Integer:
            return "integer";
        case clang::mrdox::dom::Kind::String:
            return "string";
        case clang::mrdox::dom::Kind::Array:
            return "array";
        case clang::mrdox::dom::Kind::Object:
            return "object";
    }
    return "unknown";
}

struct master_fixtures
{
    Handlebars hbs;
    dom::Object context;
    HandlebarsOptions options;
    std::string_view template_path;
    std::string template_str;
    std::string master_file_contents;
    std::vector<std::string_view> partial_paths;
    std::string_view output_path;
    std::string_view error_output_path;
    std::string master_logger_output;
    std::string_view logger_output_path;
    std::string_view logger_error_output_path;
    std::string log;
} master;

void
setup_fixtures()
{
    master.template_path =
        MRDOX_TEST_FILES_DIR "/handlebars/features_test.adoc.hbs";
    master.partial_paths = {
        MRDOX_TEST_FILES_DIR "/handlebars/record-detail.adoc.hbs",
        MRDOX_TEST_FILES_DIR "/handlebars/escaped.adoc.hbs"};
    master.output_path =
        MRDOX_TEST_FILES_DIR "/handlebars/features_test.adoc";
    master.error_output_path =
        MRDOX_TEST_FILES_DIR "/handlebars/features_test_error.adoc";
    master.logger_output_path =
        MRDOX_TEST_FILES_DIR "/handlebars/logger_output.txt";
    master.logger_error_output_path =
        MRDOX_TEST_FILES_DIR "/handlebars/logger_output_error.txt";

    Expected<std::string> template_text_r =
        files::getFileText(master.template_path);
    BOOST_TEST(template_text_r);
    master.template_str = *template_text_r;
    BOOST_TEST_NOT(master.template_str.empty());

    auto master_file_contents_r =
        files::getFileText(master.output_path);
    if (master_file_contents_r)
    {
        master.master_file_contents = *master_file_contents_r;
    }

    auto master_logger_output_r =
        files::getFileText(master.logger_output_path);
    if (master_logger_output_r)
    {
        master.master_logger_output = *master_logger_output_r;
    }

    master.options.noEscape = true;
}

void
setup_context()
{
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
    for (auto [firstname, lastname]: first_and_last_names)
    {
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
    master.context.set("page", page);
    dom::Array nav = dom::newArray<dom::DefaultArrayImpl>();
    dom::Object nav1;
    nav1.set("url", "foo");
    nav1.set("test", true);
    nav1.set("title", "bar");
    nav.emplace_back(nav1);
    dom::Object nav2;
    nav2.set("url", "bar");
    nav.emplace_back(nav2);
    master.context.set("nav", nav);
    master.context.set("myVariable", "lookupMyPartial");
    dom::Object myOtherContext;
    myOtherContext.set("information", "Interesting!");
    master.context.set("myOtherContext", myOtherContext);
    master.context.set("favoriteNumber", 123);
    master.context.set("prefix", "Hello");
    master.context.set("title", "My Title");
    master.context.set("body", "My Body");
    dom::Object story;
    story.set("intro", "Before the jump");
    story.set("body", "After the jump");
    master.context.set("story", story);
    dom::Array comments = dom::newArray<dom::DefaultArrayImpl>();
    dom::Object comment1;
    comment1.set("subject", "subject 1");
    comment1.set("body", "body 1");
    comments.emplace_back(comment1);
    dom::Object comment2;
    comment2.set("subject", "subject 2");
    comment2.set("body", "body 2");
    comments.emplace_back(comment2);
    master.context.set("comments", comments);
    master.context.set("isActive", true);
    master.context.set("isInactive", false);
    dom::Object peopleObj;
    for (auto [firstname, lastname]: first_and_last_names)
    {
            person = {};
            person.set("firstname", firstname);
            person.set("lastname", lastname);
            peopleObj.set(firstname, person);
    }
    master.context.set("peopleobj", peopleObj);
    master.context.set("author", true);
    master.context.set("firstname", "Yehuda");
    master.context.set("lastname", "Katz");
    dom::Array names = dom::newArray<dom::DefaultArrayImpl>();
    names.emplace_back("Yehuda Katz");
    names.emplace_back("Alan Johnson");
    names.emplace_back("Charles Jolley");
    master.context.set("names", names);
    dom::Object namesobj;
    namesobj.set("Yehuda", "Yehuda Katz");
    namesobj.set("Alan", "Alan Johnson");
    namesobj.set("Charles", "Charles Jolley");
    master.context.set("namesobj", namesobj);
    dom::Object city;
    city.set("name", "San Francisco");
    city.set("summary", "San Francisco is the <b>cultural center</b> of <b>Northern California</b>");
    dom::Object location;
    location.set("north", "37.73,");
    location.set("east", "-122.44");
    city.set("location", location);
    city.set("population", 883305);
    master.context.set("city", city);

    dom::Object lookup_test;
    dom::Array people_lookup = dom::newArray<dom::DefaultArrayImpl>();
    people_lookup.emplace_back("Nils");
    people_lookup.emplace_back("Yehuda");
    lookup_test.set("people", people_lookup);
    dom::Array cities_lookup = dom::newArray<dom::DefaultArrayImpl>();
    cities_lookup.emplace_back("Darmstadt");
    cities_lookup.emplace_back("San Francisco");
    lookup_test.set("cities", cities_lookup);
    master.context.set("lookup_test", lookup_test);

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
    master.context.set("lookup_test2", lookup_test2);

    dom::Object containers;
    dom::Array array;
    array.emplace_back("a");
    array.emplace_back("b");
    array.emplace_back("c");
    array.emplace_back("d");
    array.emplace_back("e");
    array.emplace_back("f");
    array.emplace_back("g");
    containers.set("array", array);

    dom::Array array2;
    array2.emplace_back("e");
    array2.emplace_back("f");
    array2.emplace_back("g");
    array2.emplace_back("h");
    array2.emplace_back("i");
    array2.emplace_back("j");
    array2.emplace_back("k");
    containers.set("array2", array2);

    dom::Object object;
    object.set("a", "a");
    object.set("b", "b");
    object.set("c", "c");
    object.set("d", "d");
    object.set("e", "e");
    object.set("f", "f");
    object.set("g", "g");
    containers.set("object", object);

    dom::Object object2;
    object2.set("e", "e");
    object2.set("f", "f");
    object2.set("g", "g");
    object2.set("h", "h");
    object2.set("i", "i");
    object2.set("j", "j");
    object2.set("k", "k");
    containers.set("object2", object2);

    dom::Array object_array;
    dom::Object account_x10;
    account_x10.set("account_id", "account-x10");
    account_x10.set("product", "Chair");
    object_array.emplace_back(account_x10);
    dom::Object account_x11;
    account_x11.set("account_id", "account-x10");
    account_x11.set("product", "Bookcase");
    object_array.emplace_back(account_x11);
    dom::Object account_x12;
    account_x12.set("account_id", "account-x11");
    account_x12.set("product", "Desk");
    object_array.emplace_back(account_x12);
    containers.set("object_array", object_array);

    master.context.set("containers", containers);
}

void
setup_helpers()
{
    helpers::registerAntoraHelpers(master.hbs);
    helpers::registerStringHelpers(master.hbs);
    helpers::registerContainerHelpers(master.hbs);

    master.hbs.registerHelper("progress", [](dom::Array const& args)
    {
        if (args.size() < 3)
        {
            return fmt::format("progress helper requires 3 arguments: {} provided", args.size());
        }
        if (!args[0].isString())
        {
            return fmt::format("progress helper requires string argument: {} received", args[0]);
        }
        if (!args[1].isInteger())
        {
            return fmt::format("progress helper requires number argument: {} received", args[1]);
        }
        if (!args[2].isBoolean())
        {
            return fmt::format("progress helper requires boolean argument: {} received", args[2]);
        }
        dom::Value nameV = args[0];
        std::string_view name = nameV.getString();
        std::uint64_t percent = args[1].getInteger();
        bool stalled = args[2].getBool();
        std::uint64_t barWidth = percent / 5;
        std::string bar = std::string(20, '*').substr(0, barWidth);
        std::string stalledStr = stalled ? "stalled" : "";
        std::string res = bar;
        res += ' ';
        res += std::to_string(percent);
        res += "% ";
        res += name;
        res += ' ';
        res += stalledStr;
        return res;
    });

    master.hbs.registerHelper("noop", helpers::noop_fn);
    master.hbs.registerHelper("raw", helpers::noop_fn);

    master.hbs.registerHelper("link", [](
        dom::Array const& args, HandlebarsCallback const& cb) -> std::string {
        if (args.empty())
        {
            return "no arguments provided to link helper";
        }
        for (std::size_t i = 1; i < args.size(); ++i)
        {
            if (!args[i].isString())
            {
                return fmt::format("link helper requires string arguments: {} provided", args.size());
            }
        }

        std::string out;
        auto h = cb.hashes().find("href");
        if (h.isString())
        {
            out += h.getString();
        }
        else if (args.size() > 1)
        {
            if (!args[1].isString())
            {
                return fmt::format("link helper requires string argument: {} provided", to_string(args[1].kind()));
            }
            auto href = args[1];
            out += href.getString();
        }
        else
        {
            out += "#";
        }

        out += '[';
        out += args[0].getString();
        // more attributes from hashes
        for (auto const& [key, value] : cb.hashes())
        {
            if (key == "href" || !value.isString())
            {
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

    master.hbs.registerHelper("loud", [](
        dom::Array const& args,
        HandlebarsCallback const& cb) -> std::string
    {
        std::string res;
        if (cb.isBlock())
        {
            res = cb.fn();
        }
        else
        {
            if (args.empty())
            {
                return "loud helper requires at least one argument";
            }
            if (!args[0].isString())
            {
                return fmt::format("loud helper requires string argument: {} provided", to_string(args[0].kind()));
            }
            res = args[0].getString();
        }
        for (char& c : res)
        {
            if (c >= 'a' && c <= 'z')
                c += 'A' - 'a';
        }
        return res;
    });

    master.hbs.registerHelper("to_string", [](
        dom::Array const& args) -> std::string {
        if (args.empty())
        {
            return "to_string helper requires at least one argument";
        }
        dom::Value arg = args[0];
        return JSON_stringify(arg);
    });

    master.hbs.registerHelper("bold", [](
        dom::Array const& /* args */,
        HandlebarsCallback const& cb) {
        return fmt::format(R"(<div class="mybold">{}</div>)", cb.fn());
    });

    master.hbs.registerHelper("list", [](
        dom::Array const& args,
        HandlebarsCallback const& cb) {
        // Built-in helper to change the context for each object in args
        if (args.size() != 1)
        {
            return fmt::format("list helper requires 1 argument: {} provided", args.size());
        }
        if (!args[0].isArray())
        {
            return fmt::format("list helper requires array argument: {} provided", to_string(args[0].kind()));
        }

        dom::Object data = createFrame(cb.data());
        dom::Value itemsV = args[0];
        dom::Array const& items = itemsV.getArray();
        if (!items.empty())
        {
            std::string out = "<ul";
            for (auto const& [key, value] : cb.hashes())
            {
                out += " ";
                out += key;
                out += "=\"";
                out += value.getString();
                out += "\"";
            }
            out += ">";
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                dom::Value item = items[i];
                // AFREITAS: this logic should be in private data
                data.set("key", static_cast<std::int64_t>(i));
                data.set("first", i == 0);
                data.set("last", i == items.size() - 1);
                data.set("index", static_cast<std::int64_t>(i));
                out += "<li>" + cb.fn(item, data, {}) + "</li>";
            }
            return out + "</ul>";
        }
        return cb.inverse();
    });

    master.hbs.registerHelper("isdefined",
        [](dom::Array const& args) -> dom::Value
    {
        if (args.empty())
        {
            return "isdefined helper requires at least one argument";
        }
        // This is an example from the handlebars.js documentation
        // There's no distinction between null and undefined in mrdox::dom
        return !args[0].isNull();
    });

    master.hbs.registerHelper("helperMissing",
        [](dom::Array const& args, HandlebarsCallback const& cb)
    {
        std::string out;
        OutputRef os(out);
        os << "Missing: ";
        os << cb.name();
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

    master.hbs.registerHelper("blockHelperMissing",
        [](dom::Array const& args, HandlebarsCallback const& cb)
    {
        std::string out;
        OutputRef os(out);
        os << "Helper '";
        os << cb.name();
        os << "' not found. Printing block: ";
        os << cb.fn();
        return out;
    });
}

void
setup_logger()
{
    master.hbs.registerLogger(
        [this](dom::Value level, dom::Array const& args)
    {
        master.log += fmt::format("[{}] ", level);
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (i != 0) {
                master.log += ", ";
            }
            master.log += args[i].getString();
        }
        master.log += '\n';
    });
}

void
setup_partials()
{
    // From files
    for (auto partial_path: master.partial_paths)
    {
        auto partial_text_r = files::getFileText(partial_path);
        BOOST_TEST(partial_text_r);
        std::string_view filename = files::getFileName(partial_path);
        auto pos = filename.find('.');
        if (pos != std::string_view::npos)
        {
            filename = filename.substr(0, pos);
        }
        master.hbs.registerPartial(filename, *partial_text_r);
    }

    // Dynamic partial helpers
    master.hbs.registerHelper("whichPartial", []() {
        return "dynamicPartial";
    });

    // Literal partials
    master.hbs.registerPartial("dynamicPartial", "Dynamo!");
    master.hbs.registerPartial("lookupMyPartial", "Found!");
    master.hbs.registerPartial("myPartialContext", "{{information}}");
    master.hbs.registerPartial("myPartialParam", "The result is {{parameter}}");
    master.hbs.registerPartial("myPartialParam2", "{{prefix}}, {{firstname}} {{lastname}}");
    master.hbs.registerPartial("layoutTemplate", "Site Content {{> @partial-block }}");
    master.hbs.registerPartial("pageLayout", "<div class=\"nav\">\n  {{> nav}}\n</div>\n<div class=\"content\">\n  {{> content}}\n</div>");
}

void
master_test()
{
    setup_fixtures();
    setup_context();
    setup_helpers();
    setup_logger();
    setup_partials();

    std::string rendered_text = master.hbs.render(
        master.template_str,
        master.context,
        master.options);
    BOOST_TEST_NOT(rendered_text.empty());

    test_suite::BOOST_TEST_DIFF(
        master.master_file_contents,
        master.output_path,
        rendered_text,
        master.error_output_path);

    test_suite::BOOST_TEST_DIFF(
        master.master_logger_output,
        master.logger_output_path,
        master.log,
        master.logger_error_output_path);
}

void
safe_string()
{
    Handlebars hbs;
    hbs.registerHelper("bold", [](dom::Array const& args) -> dom::Value
    {
        if (args.empty() || !args[0].isString()) {
            return "bold helper requires at least one argument";
        }
        std::string_view text = args[0].getString().get();
        return fmt::format("<b>{}</b>", text);
    });
    std::string templ = "{{bold 'text'}}";
    std::string res = hbs.render(templ, {});
    BOOST_TEST_NOT(res == "<b>text</b>");
    BOOST_TEST(res == "&lt;b&gt;text&lt;/b&gt;");

    HandlebarsOptions options;
    options.noEscape = true;
    res = hbs.render(templ, {}, options);
    BOOST_TEST(res == "<b>text</b>");
    BOOST_TEST_NOT(res == "&lt;b&gt;text&lt;/b&gt;");

    hbs.registerHelper("bold", [](dom::Array const& args) {
        if (args.empty() || !args[0].isString()) {
            return safeString("bold helper requires at least one argument");
        }
        std::string_view text = args[0].getString().get();
        return safeString(fmt::format("<b>{}</b>", text));
    });
    res = hbs.render(templ, {});
    BOOST_TEST(res == "<b>text</b>");
    BOOST_TEST_NOT(res == "&lt;b&gt;text&lt;/b&gt;");
}

void run()
{
    master_test();
    safe_string();
}

};

TEST_SUITE(
    Handlebars_test,
    "clang.mrdox.Handlebars");

} // mrdox
} // clang

