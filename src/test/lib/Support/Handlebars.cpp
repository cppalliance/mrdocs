//
// Copyright (c) 2023 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include <test_suite/detail/decomposer.hpp>
#include <test_suite/diff.hpp>
#include <test_suite/test_suite.hpp>
#include <fmt/format.h>
#include <mrdox/Dom.hpp>
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
        case clang::mrdox::dom::Kind::Undefined:
            return "undefined";
        case clang::mrdox::dom::Kind::Null:
            return "null";
        case clang::mrdox::dom::Kind::Boolean:
            return "boolean";
        case clang::mrdox::dom::Kind::Integer:
            return "integer";
        case clang::mrdox::dom::Kind::String:
            return "string";
        case clang::mrdox::dom::Kind::SafeString:
            return "safeString";
        case clang::mrdox::dom::Kind::Array:
            return "array";
        case clang::mrdox::dom::Kind::Object:
            return "object";
        case clang::mrdox::dom::Kind::Function:
            return "function";
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
        MRDOX_TEST_FILES_DIR "/handlebars/record.adoc.hbs",
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
    master.options.trackIds = true;
}

void
setup_context() const
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

    dom::Object symbol;
    symbol.set("tag", "struct");
    symbol.set("kind", "record");
    symbol.set("name", "T");
    master.context.set("symbol", symbol);
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
        auto h = cb.hash().find("href");
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
        for (auto const& [key, value] : cb.hash())
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
            for (auto const& [key, value] : cb.hash())
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
                out += "<li>" + cb.fn(item, data, {}, {}) + "</li>";
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

void
basic_context()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/basic.js
    Handlebars hbs;

    // most basic
    {
        dom::Object ctx;
        ctx.set("foo", "foo");
        BOOST_TEST(hbs.render("{{foo}}", ctx) == "foo");
    }

    // escaping
    {
        dom::Object ctx;
        ctx.set("foo", "food");
        BOOST_TEST(hbs.render("\\{{foo}}", ctx) == "{{foo}}");
        BOOST_TEST(hbs.render("content \\{{foo}}", ctx) == "content {{foo}}");
        BOOST_TEST(hbs.render("\\\\{{foo}}", ctx) == "\\food");
        BOOST_TEST(hbs.render("\\\\{{foo}}", ctx) == "\\food");
        BOOST_TEST(hbs.render("content \\\\{{foo}}", ctx) == "content \\food");
        BOOST_TEST(hbs.render("\\\\ {{foo}}", ctx) == "\\\\ food");
    }

    // compiling with a basic context
    {
        dom::Object ctx;
        ctx.set("cruel", "cruel");
        ctx.set("world", "world");
        BOOST_TEST(hbs.render("Goodbye\n{{cruel}}\n{{world}}!", ctx) == "Goodbye\ncruel\nworld!");
    }

    // compiling with an undefined context
    {
        dom::Value ctx = nullptr;
        BOOST_TEST(hbs.render("Goodbye\n{{cruel}}\n{{world.bar}}!", ctx) == "Goodbye\n\n!");
        BOOST_TEST(hbs.render("{{#unless foo}}Goodbye{{../test}}{{test2}}{{/unless}}", ctx) == "Goodbye");
    }

    // comments
    {
        dom::Object ctx;
        ctx.set("cruel", "cruel");
        ctx.set("world", "world");
        BOOST_TEST(hbs.render("{{! Goodbye}}Goodbye\\n{{cruel}}\\n{{world}}!", ctx) == "Goodbye\\ncruel\\nworld!");
        BOOST_TEST(hbs.render("    {{~! comment ~}}      blah", ctx) == "blah");
        BOOST_TEST(hbs.render("    {{~!-- long-comment --~}}      blah", ctx) == "blah");
        BOOST_TEST(hbs.render("    {{! comment ~}}      blah", ctx) == "    blah");
        BOOST_TEST(hbs.render("    {{!-- long-comment --~}}      blah", ctx) == "    blah");
        BOOST_TEST(hbs.render("    {{~! comment}}      blah", ctx) == "      blah");
        BOOST_TEST(hbs.render("    {{~!-- long-comment --}}      blah", ctx) == "      blah");
    }

    // boolean
    {
        std::string string = "{{#goodbye}}GOODBYE {{/goodbye}}cruel {{world}}!";
        dom::Object ctx;
        ctx.set("goodbye", true);
        ctx.set("world", "world");
        // booleans show the contents when true
        BOOST_TEST(hbs.render(string, ctx) == "GOODBYE cruel world!");
        ctx.set("goodbye", false);
        // booleans do not show the contents when false
        BOOST_TEST(hbs.render(string, ctx) == "cruel world!");
    }

    // zeros
    {
        dom::Object ctx;
        ctx.set("num1", 42);
        ctx.set("num2", 0);
        BOOST_TEST(hbs.render("num1: {{num1}}, num2: {{num2}}", ctx) == "num1: 42, num2: 0");
        BOOST_TEST(hbs.render("num: {{.}}", 0) == "num: 0");
        ctx = dom::Object();
        dom::Object num1;
        num1.set("num2", 0);
        ctx.set("num1", num1);
        BOOST_TEST(hbs.render("num: {{num1/num2}}", ctx) == "num: 0");
    }

    // false
    {
        dom::Object ctx;
        ctx.set("val1", false);
        ctx.set("val2", false);
        BOOST_TEST(hbs.render("val1: {{val1}}, val2: {{val2}}", ctx) == "val1: false, val2: false");
        BOOST_TEST(hbs.render("val: {{.}}", false) == "val: false");
        ctx = dom::Object();
        dom::Object val1;
        val1.set("val2", false);
        ctx.set("val1", val1);
        BOOST_TEST(hbs.render("val: {{val1/val2}}", ctx) == "val: false");
        ctx = dom::Object();
        ctx.set("val1", false);
        ctx.set("val2", false);
        BOOST_TEST(hbs.render("val1: {{{val1}}}, val2: {{{val2}}}", ctx) == "val1: false, val2: false");
        ctx = dom::Object();
        val1.set("val2", false);
        ctx.set("val1", val1);
        BOOST_TEST(hbs.render("val: {{{val1/val2}}}", ctx) == "val: false");
    }

    // should handle undefined and null
    {
        {
            hbs.registerHelper("awesome", [](
                dom::Array const& args, HandlebarsCallback const& cb) {
                BOOST_TEST(args.size() == 2u);
                std::string result;
                if (args[0].isNull()) {
                    result += "true ";
                }
                if (args[1].isNull()) {
                    result += "true";
                }
                return result;
            });
            dom::Object ctx;
            BOOST_TEST(hbs.render("{{awesome undefined null}}", nullptr) == "true true");
            hbs.unregisterHelper("awesome");
        }
        {
            hbs.registerHelper("undefined", [](
                dom::Array const& args, HandlebarsCallback const& cb) {
                BOOST_TEST(args.empty());
                return "undefined!";
            });
            dom::Object ctx;
            BOOST_TEST(hbs.render("{{undefined}}", nullptr) == "undefined!");
            hbs.unregisterHelper("undefined");
        }
        {
            hbs.registerHelper("null", [](
                dom::Array const& args, HandlebarsCallback const& cb) {
                BOOST_TEST(args.empty());
                return "null!";
            });
            dom::Object ctx;
            BOOST_TEST(hbs.render("{{null}}", nullptr) == "null!");
            hbs.unregisterHelper("null");
        }
    }

    // newlines
    {
        BOOST_TEST(hbs.render("Alan's\nTest") == "Alan's\nTest");
        BOOST_TEST(hbs.render("Alan's\rTest") == "Alan's\rTest");
    }

    // escaping text
    {
        BOOST_TEST(hbs.render("Awesome's") == "Awesome's");
        BOOST_TEST(hbs.render("Awesome\\") == "Awesome\\");
        BOOST_TEST(hbs.render("Awesome\\\\ foo") == "Awesome\\\\ foo");
        dom::Object ctx;
        ctx.set("foo", "\\");
        BOOST_TEST(hbs.render("Awesome {{foo}}", ctx) == "Awesome \\");
        BOOST_TEST(hbs.render(" ' ' ") == " ' ' ");
    }

    // escaping expressions
    {
        dom::Object ctx;
        ctx.set("awesome", "&'\\<>");
        // expressions with 3 handlebars aren't escaped
        BOOST_TEST(hbs.render("{{{awesome}}}", ctx) == "&'\\<>");
        // expressions with {{& handlebars aren't escaped
        BOOST_TEST(hbs.render("{{&awesome}}", ctx) == "&'\\<>");
        // by default expressions should be escaped
        ctx.set("awesome", R"(&"'`\<>)");
        BOOST_TEST(hbs.render("{{awesome}}", ctx) == "&amp;&quot;&#x27;&#x60;\\&lt;&gt;");
        // escaping should properly handle amperstands
        ctx.set("awesome", "Escaped, <b> looks like: &lt;b&gt;");
        BOOST_TEST(hbs.render("{{awesome}}", ctx) == "Escaped, &lt;b&gt; looks like: &amp;lt;b&amp;gt;");
    }

    // functions returning safestrings shouldn't be escaped
    {
        hbs.registerHelper("awesome", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            return safeString("&'\\<>");
        });
        BOOST_TEST(hbs.render("{{awesome}}") == "&'\\<>");
        hbs.unregisterHelper("awesome");
    }

    // functions
    {
        hbs.registerHelper("awesome", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            return "Awesome";
        });
        BOOST_TEST(hbs.render("{{awesome}}") == "Awesome");
        hbs.unregisterHelper("awesome");

        hbs.registerHelper("awesome", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            return cb.context().getObject().find("more");
        });
        dom::Object ctx;
        ctx.set("more", "More awesome");
        BOOST_TEST(hbs.render("{{awesome}}", ctx) == "More awesome");
        hbs.unregisterHelper("awesome");
    }

    // functions with context argument
    {
        hbs.registerHelper("awesome", [](dom::Array const& args) {
            return args[0];
        });
        dom::Object ctx;
        ctx.set("frank", "Frank");
        BOOST_TEST(hbs.render("{{awesome frank}}", ctx) == "Frank");
        hbs.unregisterHelper("awesome");
    }

    // pathed functions with context argument
    {
        // This test uses helpers in C++. The Handlebars.js test uses a
        // function in the "bar.awesome" context that is accessed as
        // "bar/awesome" from the parent context.
        hbs.registerHelper("awesome", [](dom::Array const& args) {
            return args[0];
        });
        dom::Object ctx;
        ctx.set("frank", "Frank");
        BOOST_TEST(hbs.render("{{awesome frank}}", ctx) == "Frank");
    }

    // depthed functions with context argument
    {
        // This test uses helpers in C++. The Handlebars.js test uses a
        // function in the context that is accessed as "../awesome" from
        // the "frank" context.
        hbs.registerHelper("awesome", [](dom::Array const& args) {
            return args[0];
        });
        dom::Object ctx;
        ctx.set("frank", "Frank");
        BOOST_TEST(hbs.render("{{#with frank}}{{awesome .}}{{/with}}", ctx) == "Frank");
    }

    // block functions with context argument
    {
        hbs.registerHelper("awesome", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            return cb.fn(args[0]);
        });
        BOOST_TEST(hbs.render("{{#awesome 1}}inner {{.}}{{/awesome}}") == "inner 1");
    }

    // depthed block functions with context argument
    {
        // This test uses helpers in C++. The Handlebars.js test uses a
        // function in the context that is accessed as "../awesome" from
        // the "value" context.
        hbs.registerHelper("awesome", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            return cb.fn(args[0]);
        });
        dom::Object ctx;
        ctx.set("value", true);
        BOOST_TEST(hbs.render("{{#with value}}{{#awesome 1}}inner {{.}}{{/awesome}}{{/with}}", ctx) == "inner 1");
    }

    // block functions without context argument
    {
        // block functions are called with options
        hbs.registerHelper("awesome", [](
            dom::Array const&, HandlebarsCallback const& cb) {
            return cb.fn();
        });
        BOOST_TEST(hbs.render("{{#awesome}}inner{{/awesome}}") == "inner");
    }

    // pathed block functions without context argument
    {
        // This test uses helpers in C++. The Handlebars.js test uses a
        // function in the "foo.awesome" context that is accessed as
        // "/foo/awesome" from the root context.
        hbs.registerHelper("awesome", [](
            dom::Array const&, HandlebarsCallback const& cb) {
            return cb.fn();
        });
        BOOST_TEST(hbs.render("{{#awesome}}inner{{/awesome}}") == "inner");
    }

    // depthed block functions without context argument
    {
        // This test uses helpers in C++. The Handlebars.js test uses a
        // function "awesome" in the root context that is accessed as
        // "../awesome" from the "value" context.
        hbs.registerHelper("awesome", [](
            dom::Array const&, HandlebarsCallback const& cb) {
            return cb.fn();
        });
        dom::Object ctx;
        ctx.set("value", true);
        BOOST_TEST(hbs.render("{{#with value}}{{#awesome}}inner{{/awesome}}{{/with}}", ctx) == "inner");
    }

    // paths with hyphens
    {
        dom::Object foo;
        foo.set("foo-bar", "baz");
        BOOST_TEST(hbs.render("{{foo-bar}}", foo) == "baz");

        dom::Object ctx;
        ctx.set("foo", foo);
        BOOST_TEST(hbs.render("{{foo.foo-bar}}", ctx) == "baz");
        BOOST_TEST(hbs.render("{{foo/foo-bar}}", ctx) == "baz");
    }

    // nested paths
    {
        dom::Object alan;
        alan.set("expression", "beautiful");
        dom::Object ctx;
        ctx.set("alan", alan);
        BOOST_TEST(hbs.render("Goodbye {{alan/expression}} world!", ctx) == "Goodbye beautiful world!");
    }

    // nested paths with empty string value
    {
        dom::Object alan;
        alan.set("expression", "");
        dom::Object ctx;
        ctx.set("alan", alan);
        BOOST_TEST(hbs.render("Goodbye {{alan/expression}} world!", ctx) == "Goodbye  world!");
    }

    // literal paths
    {
        dom::Object alan;
        alan.set("expression", "beautiful");
        dom::Object ctx;
        ctx.set("@alan", alan);
        BOOST_TEST(hbs.render("Goodbye {{[@alan]/expression}} world!", ctx) == "Goodbye beautiful world!");

        ctx = dom::Object();
        ctx.set("foo bar", alan);
        BOOST_TEST(hbs.render("Goodbye {{[foo bar]/expression}} world!", ctx) == "Goodbye beautiful world!");
    }

    // literal references
    {
        dom::Object ctx;
        ctx.set("foo bar", "beautiful");
        ctx.set("foo'bar", "beautiful");
        ctx.set("foo\"bar", "beautiful");
        ctx.set("foo[bar", "beautiful");
        BOOST_TEST(hbs.render("Goodbye {{[foo bar]}} world!", ctx) == "Goodbye beautiful world!");
        BOOST_TEST(hbs.render("Goodbye {{\"foo bar\"}} world!", ctx) == "Goodbye beautiful world!");
        BOOST_TEST(hbs.render("Goodbye {{'foo bar'}} world!", ctx) == "Goodbye beautiful world!");
        BOOST_TEST(hbs.render("Goodbye {{\"foo[bar\"}} world!", ctx) == "Goodbye beautiful world!");
        BOOST_TEST(hbs.render("Goodbye {{\"foo'bar\"}} world!", ctx) == "Goodbye beautiful world!");
        BOOST_TEST(hbs.render("Goodbye {{'foo\"bar'}} world!", ctx) == "Goodbye beautiful world!");
    }

    // that current context path ({{.}}) doesn't hit helpers
    {
        hbs.registerHelper("awesome", [](
            dom::Array const&, HandlebarsCallback const& cb) {
            return cb.fn();
        });
        BOOST_TEST(hbs.render("test: {{.}}", nullptr) == "test: ");
    }

    // complex but empty paths
    {
        dom::Object person;
        person.set("name", nullptr);
        dom::Object ctx;
        ctx.set("person", person);
        BOOST_TEST(hbs.render("{{person/name}}", ctx).empty());

        ctx = dom::Object();
        ctx.set("person", dom::Object());
        BOOST_TEST(hbs.render("{{person/name}}", ctx).empty());
    }

    // this keyword in paths
    {
        dom::Object ctx;
        dom::Array goodbyes;
        goodbyes.emplace_back("goodbye");
        goodbyes.emplace_back("Goodbye");
        goodbyes.emplace_back("GOODBYE");
        ctx.set("goodbyes", goodbyes);
        BOOST_TEST(hbs.render("{{#goodbyes}}{{this}}{{/goodbyes}}", ctx) == "goodbyeGoodbyeGOODBYE");

        dom::Array hellos;
        dom::Object hello1;
        hello1.set("text", "hello");
        hellos.emplace_back(hello1);
        dom::Object hello2;
        hello2.set("text", "Hello");
        hellos.emplace_back(hello2);
        dom::Object hello3;
        hello3.set("text", "HELLO");
        hellos.emplace_back(hello3);
        ctx.set("hellos", hellos);
        BOOST_TEST(hbs.render("{{#hellos}}{{this/text}}{{/hellos}}", ctx) == "helloHelloHELLO");
    }

    // this keyword nested inside path
    {
        BOOST_TEST_THROW_WITH(
            hbs.render("{{text/this/foo}}"),
            HandlebarsError, "Invalid path: text/this - 1:2");
        dom::Object ctx;
        dom::Array hellos;
        dom::Object hello1;
        hello1.set("text", "hello");
        hellos.emplace_back(hello1);
        ctx.set("hellos", hellos);
        hbs.registerHelper("foo", [](dom::Array const& args) {
            return args[0];
        });
        BOOST_TEST_THROW_WITH(
            hbs.render("{{#hellos}}{{foo text/this/foo}}{{/hellos}}", ctx),
            HandlebarsError, "Invalid path: text/this - 1:17");
        ctx.set("this", "bar");
        BOOST_TEST(hbs.render("{{foo [this]}}", ctx) == "bar");
        dom::Object this_obj;
        this_obj.set("this", "bar");
        ctx.set("text", this_obj);
        BOOST_TEST(hbs.render("{{foo text/[this]}}", ctx) == "bar");
    }

    // this keyword in helpers
    {
        hbs.registerHelper("foo", [](dom::Array const& args) {
            std::string res = "bar ";
            res += args[0].getString();
            return res;
        });

        // This keyword in paths evaluates to current context
        dom::Object ctx;
        dom::Array goodbyes;
        goodbyes.emplace_back("goodbye");
        goodbyes.emplace_back("Goodbye");
        goodbyes.emplace_back("GOODBYE");
        ctx.set("goodbyes", goodbyes);
        BOOST_TEST(hbs.render("{{#goodbyes}}{{foo this}}{{/goodbyes}}", ctx) == "bar goodbyebar Goodbyebar GOODBYE");

        // This keyword evaluates in more complex paths
        dom::Array hellos;
        dom::Object hello1;
        hello1.set("text", "hello");
        hellos.emplace_back(hello1);
        dom::Object hello2;
        hello2.set("text", "Hello");
        hellos.emplace_back(hello2);
        dom::Object hello3;
        hello3.set("text", "HELLO");
        hellos.emplace_back(hello3);
        ctx.set("hellos", hellos);
        BOOST_TEST(hbs.render("{{#hellos}}{{foo this/text}}{{/hellos}}", ctx) == "bar hellobar Hellobar HELLO");
    }

    // this keyword nested inside helpers param
    {
        hbs.registerHelper("foo", [](dom::Array const& args) {
            return args[0];
        });
        dom::Object ctx;
        dom::Array hellos;
        dom::Object hello1;
        hello1.set("text", "hello");
        hellos.emplace_back(hello1);
        ctx.set("hellos", hellos);
        BOOST_TEST_THROW_WITH(
            hbs.render("{{#hellos}}{{foo text/this/foo}}{{/hellos}}", ctx),
            HandlebarsError, "Invalid path: text/this - 1:17");

        ctx.set("this", "bar");
        BOOST_TEST(hbs.render("{{foo [this]}}", ctx) == "bar");

        dom::Object this_obj;
        this_obj.set("this", "bar");
        ctx.set("text", this_obj);
        BOOST_TEST(hbs.render("{{foo text/[this]}}", ctx) == "bar");

        hbs.unregisterHelper("foo");
    }

    // pass string literals
    {
        BOOST_TEST(hbs.render("{{\"foo\"}}").empty());

        dom::Object ctx;
        ctx.set("foo", "bar");
        BOOST_TEST(hbs.render("{{\"foo\"}}", ctx) == "bar");

        ctx = dom::Object();
        dom::Array foo;
        foo.emplace_back("bar");
        foo.emplace_back("baz");
        ctx.set("foo", foo);
        BOOST_TEST(hbs.render("{{#\"foo\"}}{{.}}{{/\"foo\"}}", ctx) == "barbaz");
    }

    // pass number literals
    {
        BOOST_TEST(hbs.render("{{12}}").empty());

        dom::Object ctx;
        ctx.set("12", "bar");
        BOOST_TEST(hbs.render("{{12}}", ctx) == "bar");

        BOOST_TEST(hbs.render("{{12.34}}").empty());

        ctx = dom::Object();
        ctx.set("12.34", "bar");
        BOOST_TEST(hbs.render("{{12.34}}", ctx) == "bar");

        hbs.registerHelper("12.34", [](dom::Array const& args) {
            std::string res = "bar";
            res += std::to_string(args[0].getInteger());
            return res;
        });
        BOOST_TEST(hbs.render("{{12.34 1}}") == "bar1");
        hbs.unregisterHelper("12.34");
    }

    // pass boolean literals
    {
        BOOST_TEST(hbs.render("{{true}}").empty());

        dom::Object ctx;
        ctx.set("", "foo");
        BOOST_TEST(hbs.render("{{true}}").empty());

        ctx = dom::Object();
        ctx.set("false", "foo");
        BOOST_TEST(hbs.render("{{false}}", ctx) == "foo");
    }

    // should handle literals in subexpression
    {
        hbs.registerHelper("foo", [](dom::Array const& args) {
            return args[0];
        });
        hbs.registerHelper("false", []() {
            return "bar";
        });
        BOOST_TEST(hbs.render("{{foo (false)}}") == "bar");
    }
}

void
whitespace_control()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/whitespace-control.js
    Handlebars hbs;
    dom::Object hash;
    hash.set("foo", "bar<");

    // should strip whitespace around mustache calls
    {
        BOOST_TEST(hbs.render(" {{~foo~}} ", hash) == "bar&lt;");
        BOOST_TEST(hbs.render(" {{~foo}} ", hash) == "bar&lt; ");
        BOOST_TEST(hbs.render(" {{foo~}} ", hash) == " bar&lt;");
        BOOST_TEST(hbs.render(" {{~&foo~}} ", hash) == "bar<");
        BOOST_TEST(hbs.render(" {{~{foo}~}} ", hash) == "bar<");
        BOOST_TEST(hbs.render("1\n{{foo~}} \n\n 23\n{{bar}}4") == "1\n23\n4");
    }

    // blocks
    {
        // should strip whitespace around simple block calls
        {
            BOOST_TEST(hbs.render(" {{~#if foo~}} bar {{~/if~}} ", hash) == "bar");
            BOOST_TEST(hbs.render(" {{#if foo~}} bar {{/if~}} ", hash) == " bar ");
            BOOST_TEST(hbs.render(" {{~#if foo}} bar {{~/if}} ", hash) == " bar ");
            BOOST_TEST(hbs.render(" {{#if foo}} bar {{/if}} ", hash) == "  bar  ");
            BOOST_TEST(hbs.render(" \n\n{{~#if foo~}} \n\nbar \n\n{{~/if~}}\n\n ", hash) == "bar");
            BOOST_TEST(hbs.render(" a\n\n{{~#if foo~}} \n\nbar \n\n{{~/if~}}\n\na ", hash) == " abara ");
        }

        // should strip whitespace around inverse block calls
        {
            BOOST_TEST(hbs.render(" {{~^if foo~}} bar {{~/if~}} ") == "bar");
            BOOST_TEST(hbs.render(" {{^if foo~}} bar {{/if~}} ") == " bar ");
            BOOST_TEST(hbs.render(" {{~^if foo}} bar {{~/if}} ") == " bar ");
            BOOST_TEST(hbs.render(" {{^if foo}} bar {{/if}} ") == "  bar  ");
            BOOST_TEST(hbs.render(" \n\n{{~^if foo~}} \n\nbar \n\n{{~/if~}}\n\n ") == "bar");
        }

        // should strip whitespace around complex block calls
        {
            BOOST_TEST(hbs.render("{{#if foo~}} bar {{~^~}} baz {{~/if}}", hash) == "bar");
            BOOST_TEST(hbs.render("{{#if foo~}} bar {{^~}} baz {{/if}}", hash) == "bar ");
            BOOST_TEST(hbs.render("{{#if foo}} bar {{~^~}} baz {{~/if}}", hash) == " bar");
            BOOST_TEST(hbs.render("{{#if foo}} bar {{^~}} baz {{/if}}", hash) == " bar ");
            BOOST_TEST(hbs.render("{{#if foo~}} bar {{~else~}} baz {{~/if}}", hash) == "bar");
            BOOST_TEST(hbs.render("\n\n{{~#if foo~}} \n\nbar \n\n{{~^~}} \n\nbaz \n\n{{~/if~}}\n\n", hash) == "bar");
            BOOST_TEST(hbs.render("\n\n{{~#if foo~}} \n\n{{{foo}}} \n\n{{~^~}} \n\nbaz \n\n{{~/if~}}\n\n", hash) == "bar<");
            BOOST_TEST(hbs.render("{{#if foo~}} bar {{~^~}} baz {{~/if}}") == "baz");
            BOOST_TEST(hbs.render("{{#if foo}} bar {{~^~}} baz {{/if}}") == "baz ");
            BOOST_TEST(hbs.render("{{#if foo~}} bar {{~^}} baz {{~/if}}") == " baz");
            BOOST_TEST(hbs.render("{{#if foo~}} bar {{~^}} baz {{/if}}") == " baz ");
            BOOST_TEST(hbs.render("{{#if foo~}} bar {{~else~}} baz {{~/if}}") == "baz");
            BOOST_TEST(hbs.render("\n\n{{~#if foo~}} \n\nbar \n\n{{~^~}} \n\nbaz \n\n{{~/if~}}\n\n") == "baz");
        }
    }

    // should strip whitespace around partials
    {
        hbs.registerPartial("dude", "bar");
        BOOST_TEST(hbs.render("foo {{~> dude~}} ") == "foobar");
        BOOST_TEST(hbs.render("foo {{> dude~}} ") == "foo bar");
        BOOST_TEST(hbs.render("foo {{> dude}} ") == "foo bar ");
        BOOST_TEST(hbs.render("foo\n {{~> dude}} ") == "foobar");
        BOOST_TEST(hbs.render("foo\n {{> dude}} ") == "foo\n bar");
    }

    // should only strip whitespace once
    {
        dom::Object ctx;
        ctx.set("foo", "bar");
        BOOST_TEST(hbs.render(" {{~foo~}} {{foo}} {{foo}} ", ctx) == "barbar bar ");
    }
}

void
partials()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/partials.js

    Handlebars hbs;
    HandlebarsOptions emptyDataOptions;
    emptyDataOptions.data = false;

    dom::Object hash;
    dom::Array dudes;
    dom::Object dude1;
    dude1.set("name", "Yehuda");
    dude1.set("url", "http://yehuda");
    dudes.emplace_back(dude1);
    dom::Object dude2;
    dude2.set("name", "Alan");
    dude2.set("url", "http://alan");
    dudes.emplace_back(dude2);
    hash.set("dudes", dudes);

    // basic partials
    {
        std::string str = "Dudes: {{#dudes}}{{> dude}}{{/dudes}}";
        std::string partial = "{{name}} ({{url}}) ";

        hbs.registerPartial("dude", partial);
        BOOST_TEST(hbs.render(str, hash) == "Dudes: Yehuda (http://yehuda) Alan (http://alan) ");
        BOOST_TEST(hbs.render(str, hash, emptyDataOptions) == "Dudes: Yehuda (http://yehuda) Alan (http://alan) ");
    }

    // dynamic partials
    {
        std::string str = "Dudes: {{#dudes}}{{> (partial)}}{{/dudes}}";
        std::string partial = "{{name}} ({{url}}) ";
        hbs.registerHelper("partial", []() {
            return "dude";
        });
        hbs.registerPartial("dude", partial);
        BOOST_TEST(hbs.render(str, hash) == "Dudes: Yehuda (http://yehuda) Alan (http://alan) ");
        BOOST_TEST(hbs.render(str, hash, emptyDataOptions) == "Dudes: Yehuda (http://yehuda) Alan (http://alan) ");
        hbs.unregisterPartial("dude");
    }

    // failing dynamic partials
    {
        std::string str = "Dudes: {{#dudes}}{{> (partial)}}{{/dudes}}";
        std::string partial = "{{name}} ({{url}}) ";
        hbs.registerHelper("partial", []() {
            return "missing";
        });
        hbs.registerPartial("dude", partial);
        BOOST_TEST_THROW_WITH(
            hbs.render(str, hash),
            HandlebarsError, "The partial missing could not be found");
    }

    // partials with context
    {
        // Partials can be passed a context
        std::string str = "Dudes: {{>dude dudes}}";
        hbs.registerPartial("dude", "{{#this}}{{name}} ({{url}}) {{/this}}");
        BOOST_TEST(hbs.render(str, hash) == "Dudes: Yehuda (http://yehuda) Alan (http://alan) ");
    }

    // partials with no context
    {
        hbs.registerPartial("dude", "{{name}} ({{url}}) ");
        HandlebarsOptions opt2;
        opt2.explicitPartialContext = true;
        BOOST_TEST(hbs.render("Dudes: {{#dudes}}{{>dude}}{{/dudes}}", hash, opt2) == "Dudes:  ()  () ");
        BOOST_TEST(hbs.render("Dudes: {{#dudes}}{{>dude name=\"foo\"}}{{/dudes}}", hash, opt2) == "Dudes: foo () foo () ");
    }

    // partials with string context
    {
        hbs.registerPartial("dude", "{{.}}");
        BOOST_TEST(hbs.render("Dudes: {{>dude \"dudes\"}}") == "Dudes: dudes");
    }

    // partials with undefined context
    {
        hbs.registerPartial("dude", "{{foo}} Empty");
        BOOST_TEST(hbs.render("Dudes: {{>dude dudes}}") == "Dudes:  Empty");
    }

    // partials with duplicate parameters
    {
        BOOST_TEST_THROW_WITH(
            hbs.render("Dudes: {{>dude dudes foo bar=baz}}"),
            HandlebarsError,
            "Unsupported number of partial arguments: 2 - 1:7");
    }

    // partials with parameters
    {
        // Basic partials output based on current context.
        hash.set("foo", "bar");
        hbs.registerPartial("dude", "{{others.foo}}{{name}} ({{url}}) ");
        BOOST_TEST(
            hbs.render("Dudes: {{#dudes}}{{> dude others=..}}{{/dudes}}", hash) ==
            "Dudes: barYehuda (http://yehuda) barAlan (http://alan) ");
    }

    // partial in a partial
    {
        hbs.registerPartial("dude", "{{name}} {{> url}} ");
        hbs.registerPartial("url", "<a href=\"{{url}}\">{{url}}</a>");
        BOOST_TEST(
            hbs.render("Dudes: {{#dudes}}{{>dude}}{{/dudes}}", hash) ==
            "Dudes: Yehuda <a href=\"http://yehuda\">http://yehuda</a> Alan <a href=\"http://alan\">http://alan</a> ");
    }

    // rendering undefined partial throws an exception
    {
        BOOST_TEST_THROW_WITH(
            hbs.render("{{> whatever}}"),
            HandlebarsError, "The partial whatever could not be found");
    }

    // registering undefined partial throws an exception
    {
        // Nothing to test since this is a type error in C++.
    }

    // rendering function partial in vm mode
    {
        // Unsupported by this implementation.
    }

    // a partial preceding a selector
    {
        // Regular selectors can follow a partial
        dom::Object ctx;
        ctx.set("name", "Jeepers");
        ctx.set("anotherDude", "Creepers");
        hbs.registerPartial("dude", "{{name}}");
        BOOST_TEST(
            hbs.render("Dudes: {{>dude}} {{anotherDude}}", ctx) ==
            "Dudes: Jeepers Creepers");
    }

    // Partials with slash paths
    {
        dom::Object ctx;
        ctx.set("name", "Jeepers");
        ctx.set("anotherDude", "Creepers");
        hbs.registerPartial("shared/dude", "{{name}}");
        BOOST_TEST(
            hbs.render("Dudes: {{> shared/dude}}", ctx) ==
            "Dudes: Jeepers");
    }

    // Partials with slash and point paths
    {
        dom::Object ctx;
        ctx.set("name", "Jeepers");
        ctx.set("anotherDude", "Creepers");
        hbs.registerPartial("shared/dude.thing", "{{name}}");
        BOOST_TEST(
            hbs.render("Dudes: {{> shared/dude.thing}}", ctx) ==
            "Dudes: Jeepers");
    }

    // Global Partials
    {
        // There's no global environment in this implementation
        dom::Object ctx;
        ctx.set("name", "Jeepers");
        ctx.set("anotherDude", "Creepers");
        hbs.registerPartial("shared/dude", "{{name}}");
        hbs.registerPartial("globalTest", "{{anotherDude}}");
        BOOST_TEST(
            hbs.render("Dudes: {{> shared/dude}} {{> globalTest}}", ctx) ==
            "Dudes: Jeepers Creepers");
    }

    // Multiple partial registration
    {
        // This feature is not supported by this implementation.
    }

    // Partials with integer path
    {
        dom::Object ctx;
        ctx.set("name", "Jeepers");
        ctx.set("anotherDude", "Creepers");
        hbs.registerPartial("404", "{{name}}");
        BOOST_TEST(
            hbs.render("Dudes: {{> 404}}", ctx) ==
            "Dudes: Jeepers");
    }

    // Partials with complex path
    {
        dom::Object ctx;
        ctx.set("name", "Jeepers");
        ctx.set("anotherDude", "Creepers");
        hbs.registerPartial("404/asdf?.bar", "{{name}}");
        BOOST_TEST(
            hbs.render("Dudes: {{> 404/asdf?.bar}}", ctx) ==
            "Dudes: Jeepers");
    }

    // Partials with string
    {
        dom::Object ctx;
        ctx.set("name", "Jeepers");
        ctx.set("anotherDude", "Creepers");
        hbs.registerPartial("+404/asdf?.bar", "{{name}}");
        BOOST_TEST(
            hbs.render("Dudes: {{> '+404/asdf?.bar'}}", ctx) ==
            "Dudes: Jeepers");
    }

    // should handle empty partial
    {
        hbs.registerPartial("dude", "");
        BOOST_TEST(
            hbs.render("Dudes: {{#dudes}}{{> dude}}{{/dudes}}", hash) ==
            "Dudes: ");
    }

    // throw on missing partial
    {
        hbs.unregisterPartial("dude");
        BOOST_TEST_THROW_WITH(
            hbs.render("{{> dude}}", hash),
            HandlebarsError, "The partial dude could not be found");
    }
}

void
partial_blocks()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/partials.js

    Handlebars hbs;

    // should render partial block as default
    {
        BOOST_TEST(hbs.render("{{#> dude}}success{{/dude}}") == "success");
    }

    // should execute default block with proper context
    {
        dom::Object context;
        context.set("value", "success");
        BOOST_TEST(hbs.render("{{#> dude context}}{{value}}{{/dude}}", context) == "success");
    }

    // should propagate block parameters to default block
    {
        dom::Object context;
        dom::Object value;
        value.set("value", "success");
        context.set("context", value);
        BOOST_TEST(hbs.render("{{#with context as |me|}}{{#> dude}}{{me.value}}{{/dude}}{{/with}}", context) == "success");
    }

    // should not use partial block if partial exists
    {
        hbs.registerPartial("dude", "success");
        BOOST_TEST(hbs.render("{{#> dude}}fail{{/dude}}") == "success");
    }

    // should render block from partial
    {
        hbs.registerPartial("dude", "{{> @partial-block }}");
        BOOST_TEST(hbs.render("{{#> dude}}success{{/dude}}") == "success");
    }

    // should be able to render the partial-block twice
    {
        hbs.registerPartial("dude", "{{> @partial-block }} {{> @partial-block }}");
        BOOST_TEST(hbs.render("{{#> dude}}success{{/dude}}") == "success success");
    }

    // should render block from partial with context
    {
        dom::Object value;
        value.set("value", "success");
        dom::Object ctx;
        ctx.set("context", value);
        hbs.registerPartial("dude", "{{#with context}}{{> @partial-block }}{{/with}}");
        BOOST_TEST(hbs.render("{{#> dude}}{{value}}{{/dude}}", ctx) == "success");
    }

    // should be able to access the @data frame from a partial-block
    {
        dom::Object ctx;
        ctx.set("value", "success");
        hbs.registerPartial("dude", "<code>before-block: {{@root/value}} {{>   @partial-block }}</code>");
        BOOST_TEST(
            hbs.render("{{#> dude}}in-block: {{@root/value}}{{/dude}}", ctx) ==
            "<code>before-block: success in-block: success</code>");
    }

    // should allow the #each-helper to be used along with partial-blocks
    {
        dom::Object ctx;
        dom::Array values;
        values.emplace_back("a");
        values.emplace_back("b");
        values.emplace_back("c");
        ctx.set("value", values);
        hbs.registerPartial("list", "<list>{{#each .}}<item>{{> @partial-block}}</item>{{/each}}</list>");
        BOOST_TEST(
            hbs.render("<template>{{#> list value}}value = {{.}}{{/list}}</template>", ctx) ==
            "<template><list><item>value = a</item><item>value = b</item><item>value = c</item></list></template>");
    }

    // should render block from partial with context (twice)
    {
        dom::Object value;
        value.set("value", "success");
        dom::Object ctx;
        ctx.set("context", value);
        hbs.registerPartial("dude", "{{#with context}}{{> @partial-block }} {{> @partial-block }}{{/with}}");
        BOOST_TEST(hbs.render("{{#> dude}}{{value}}{{/dude}}", ctx) == "success success");
    }

    // should render block from partial with context
    {
        // { context: { value: 'success' } }
        dom::Object ctx;
        dom::Object value;
        value.set("value", "success");
        ctx.set("context", value);
        hbs.registerPartial("dude", "{{#with context}}{{> @partial-block }}{{/with}}");
        BOOST_TEST(hbs.render("{{#> dude}}{{../context/value}}{{/dude}}", ctx) == "success");
    }

    // should render block from partial with block params
    {
        dom::Object value;
        value.set("value", "success");
        dom::Object ctx;
        ctx.set("context", value);
        hbs.registerPartial("dude", "{{> @partial-block }}");
        BOOST_TEST(hbs.render("{{#with context as |me|}}{{#> dude}}{{me.value}}{{/dude}}{{/with}}", ctx) == "success");
    }

    // should render nested partial blocks
    {
        dom::Object value;
        value.set("value", "success");
        hbs.registerPartial("outer", "<outer>{{#> nested}}<outer-block>{{> @partial-block}}</outer-block>{{/nested}}</outer>");
        hbs.registerPartial("nested", "<nested>{{> @partial-block}}</nested>");
        BOOST_TEST(
            hbs.render("<template>{{#> outer}}{{value}}{{/outer}}</template>", value) ==
            "<template><outer><nested><outer-block>success</outer-block></nested></outer></template>");
    }

    // should render nested partial blocks at different nesting levels
    {
        dom::Object value;
        value.set("value", "success");
        hbs.registerPartial("outer", "<outer>{{#> nested}}<outer-block>{{> @partial-block}}</outer-block>{{/nested}}{{> @partial-block}}</outer>");
        hbs.registerPartial("nested", "<nested>{{> @partial-block}}</nested>");
        BOOST_TEST(
            hbs.render("<template>{{#> outer}}{{value}}{{/outer}}</template>", value) ==
            "<template><outer><nested><outer-block>success</outer-block></nested>success</outer></template>");
    }

    // should render nested partial blocks at different nesting levels (twice)
    {
        dom::Object value;
        value.set("value", "success");
        hbs.registerPartial("outer", "<outer>{{#> nested}}<outer-block>{{> @partial-block}} {{> @partial-block}}</outer-block>{{/nested}}{{> @partial-block}}+{{> @partial-block}}</outer>");
        hbs.registerPartial("nested", "<nested>{{> @partial-block}}</nested>");
        BOOST_TEST(
            hbs.render("<template>{{#> outer}}{{value}}{{/outer}}</template>", value) ==
            "<template><outer><nested><outer-block>success success</outer-block></nested>success+success</outer></template>");
    }

    // should render nested partial blocks (twice at each level)
    {
        dom::Object value;
        value.set("value", "success");
        hbs.registerPartial("outer", "<outer>{{#> nested}}<outer-block>{{> @partial-block}} {{> @partial-block}}</outer-block>{{/nested}}</outer>");
        hbs.registerPartial("nested", "<nested>{{> @partial-block}}{{> @partial-block}}</nested>");
        BOOST_TEST(
            hbs.render("<template>{{#> outer}}{{value}}{{/outer}}</template>", value) ==
            "<template><outer><nested><outer-block>success success</outer-block><outer-block>success success</outer-block></nested></outer></template>");
    }
}

void
inline_partials()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/partials.js
    Handlebars hbs;

    // should define inline partials for template
    {
        BOOST_TEST(
            hbs.render("{{#*inline \"myPartial\"}}success{{/inline}}{{> myPartial}}") ==
            "success");
    }

    // should overwrite multiple partials in the same template
    {
        BOOST_TEST(
            hbs.render("{{#*inline \"myPartial\"}}fail{{/inline}}{{#*inline \"myPartial\"}}success{{/inline}}{{> myPartial}}") ==
            "success");
    }

    // should define inline partials for block
    {
        BOOST_TEST(
            hbs.render("{{#with .}}{{#*inline \"myPartial\"}}success{{/inline}}{{> myPartial}}{{/with}}") ==
            "success");

        BOOST_TEST_THROW_WITH(
            hbs.render("{{#with .}}{{#*inline \"myPartial\"}}success{{/inline}}{{/with}}{{> myPartial}}"),
            HandlebarsError, "The partial myPartial could not be found");
    }

    // should override global partials
    {
        hbs.registerPartial("myPartial", "fail");
        BOOST_TEST(
            hbs.render("{{#*inline \"myPartial\"}}success{{/inline}}{{> myPartial}}") ==
            "success");
        hbs.unregisterPartial("myPartial");
    }

    // should override template partials
    {
        BOOST_TEST(
            hbs.render("{{#*inline \"myPartial\"}}fail{{/inline}}{{#with .}}{{#*inline \"myPartial\"}}success{{/inline}}{{> myPartial}}{{/with}}") ==
            "success");
    }

    // should override partials down the entire stack
    {
        BOOST_TEST(
            hbs.render("{{#with .}}{{#*inline \"myPartial\"}}success{{/inline}}{{#with .}}{{#with .}}{{> myPartial}}{{/with}}{{/with}}{{/with}}") ==
            "success");
    }

    // should define inline partials for partial call
    {
        hbs.registerPartial("dude", "{{> myPartial }}");
        BOOST_TEST(
            hbs.render("{{#*inline \"myPartial\"}}success{{/inline}}{{> dude}}") ==
            "success");
        hbs.unregisterPartial("dude");
    }

    // should define inline partials in partial block call
    {
        hbs.registerPartial("dude", "{{> myPartial }}");
        BOOST_TEST(
            hbs.render("{{#> dude}}{{#*inline \"myPartial\"}}success{{/inline}}{{/dude}}") ==
            "success");
        hbs.unregisterPartial("dude");
    }

    // should render nested inline partials
    {
        dom::Object ctx;
        ctx.set("value", "success");
        BOOST_TEST(
            hbs.render(
                "{{#*inline \"outer\"}}{{#>inner}}<outer-block>{{>@partial-block}}</outer-block>{{/inner}}{{/inline}}"
                "{{#*inline \"inner\"}}<inner>{{>@partial-block}}</inner>{{/inline}}"
                "{{#>outer}}{{value}}{{/outer}}", ctx) ==
                "<inner><outer-block>success</outer-block></inner>");
    }

    // should render nested inline partials with partial-blocks on different nesting levels
    {
        dom::Object ctx;
        ctx.set("value", "success");
        BOOST_TEST(
            hbs.render(
                "{{#*inline \"outer\"}}{{#>inner}}<outer-block>{{>@partial-block}}</outer-block>{{/inner}}{{>@partial-block}}{{/inline}}"
                "{{#*inline \"inner\"}}<inner>{{>@partial-block}}</inner>{{/inline}}"
                "{{#>outer}}{{value}}{{/outer}}", ctx) ==
            "<inner><outer-block>success</outer-block></inner>success");
        // {{#>outer}}{{value}}{{/outer}}
        // {{#>inner}}<outer-block>{{value}}</outer-block>{{/inner}}{{value}}
        // <inner><outer-block>{{value}}</outer-block>{{/inner}}</inner>{{value}}
        // <inner><outer-block>success</outer-block>{{/inner}}</inner>success
    }

    // should render nested inline partials (twice at each level)
    {
        dom::Object ctx;
        ctx.set("value", "success");
        BOOST_TEST(
            hbs.render(
                "{{#*inline \"outer\"}}{{#>inner}}<outer-block>{{>@partial-block}} {{>@partial-block}}</outer-block>{{/inner}}{{/inline}}"
                "{{#*inline \"inner\"}}<inner>{{>@partial-block}}{{>@partial-block}}</inner>{{/inline}}"
                "{{#>outer}}{{value}}{{/outer}}", ctx) ==
            "<inner><outer-block>success success</outer-block><outer-block>success success</outer-block></inner>");
    }
}

void
standalone_partials()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/partials.js
    Handlebars hbs;

    dom::Object hash;
    dom::Array dudes;
    dom::Object dude1;
    dude1.set("name", "Yehuda");
    dude1.set("url", "http://yehuda");
    dudes.emplace_back(dude1);
    dom::Object dude2;
    dude2.set("name", "Alan");
    dude2.set("url", "http://alan");
    dudes.emplace_back(dude2);
    hash.set("dudes", dudes);

    // indented partials
    {
        hbs.registerPartial("dude", "{{name}}\n");
        BOOST_TEST(
            hbs.render("Dudes:\n{{#dudes}}\n  {{>dude}}\n{{/dudes}}", hash) ==
            "Dudes:\n  Yehuda\n  Alan\n");
    }

    // nested indented partials
    {
        hbs.registerPartial("dude", "{{name}}\n {{> url}}");
        hbs.registerPartial("url", "{{url}}!\n");
        BOOST_TEST(
            hbs.render("Dudes:\n{{#dudes}}\n  {{>dude}}\n{{/dudes}}", hash) ==
            "Dudes:\n  Yehuda\n   http://yehuda!\n  Alan\n   http://alan!\n");
    }

    // prevent nested indented partials
    {
        hbs.registerPartial("dude", "{{name}}\n {{> url}}");
        hbs.registerPartial("url", "{{url}}!\n");
        HandlebarsOptions opt;
        opt.preventIndent = true;
        BOOST_TEST(
            hbs.render("Dudes:\n{{#dudes}}\n  {{>dude}}\n{{/dudes}}", hash, opt) ==
            "Dudes:\n  Yehuda\n http://yehuda!\n  Alan\n http://alan!\n");
    }
}

void
partial_compat_mode()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/partials.js
    Handlebars hbs;

    // { root: 'yes',
    //   dudes: [
    //       { name: 'Yehuda', url: 'http://yehuda' },
    //       { name: 'Alan', url: 'http://alan' }
    //   ]}
    dom::Object root;
    root.set("root", "yes");
    dom::Array dudes;
    dom::Object dude1;
    dude1.set("name", "Yehuda");
    dude1.set("url", "http://yehuda");
    dudes.emplace_back(dude1);
    dom::Object dude2;
    dude2.set("name", "Alan");
    dude2.set("url", "http://alan");
    dudes.emplace_back(dude2);
    root.set("dudes", dudes);

    HandlebarsOptions compat;
    compat.compat = true;

    // partials can access parents
    {
        hbs.registerPartial("dude", "{{name}} ({{url}}) {{root}} ");
        BOOST_TEST(
            hbs.render("Dudes: {{#dudes}}{{> dude}}{{/dudes}}", root, compat) ==
            "Dudes: Yehuda (http://yehuda) yes Alan (http://alan) yes ");
    }

    // partials can access parents with custom context
    {
        hbs.registerPartial("dude", "{{name}} ({{url}}) {{root}} ");
        BOOST_TEST(
            hbs.render("Dudes: {{#dudes}}{{> dude \"test\"}}{{/dudes}}", root, compat) ==
            "Dudes: Yehuda (http://yehuda) yes Alan (http://alan) yes ");
    }

    // partials can access parents without data
    {
        hbs.registerPartial("dude", "{{name}} ({{url}}) {{root}} ");
        compat.data = false;
        BOOST_TEST(
            hbs.render("Dudes: {{#dudes}}{{> dude}}{{/dudes}}", root, compat) ==
            "Dudes: Yehuda (http://yehuda) yes Alan (http://alan) yes ");
        compat.data = nullptr;
    }

    // partials inherit compat
    {
        hbs.registerPartial("dude", "{{#dudes}}{{name}} ({{url}}) {{root}} {{/dudes}}");
        BOOST_TEST(
            hbs.render("Dudes: {{> dude}}", root, compat) ==
            "Dudes: Yehuda (http://yehuda) yes Alan (http://alan) yes ");
    }
}

void
blocks()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/blocks.js
    Handlebars hbs;

    dom::Object ctx;
    dom::Array goodbyes;
    dom::Object goodbye1;
    goodbye1.set("text", "goodbye");
    goodbyes.emplace_back(goodbye1);
    dom::Object goodbye2;
    goodbye2.set("text", "Goodbye");
    goodbyes.emplace_back(goodbye2);
    dom::Object goodbye3;
    goodbye3.set("text", "GOODBYE");
    goodbyes.emplace_back(goodbye3);
    ctx.set("goodbyes", goodbyes);
    ctx.set("world", "world");
    ctx.set("name", "Alan");

    dom::Object emptyCtx;
    dom::Array emptyGoodbyes;
    emptyCtx.set("goodbyes", emptyGoodbyes);
    emptyCtx.set("world", "world");
    emptyCtx.set("name", "Alan");

    // array
    {
        // Arrays iterate over the contents when not empty
        BOOST_TEST(
            hbs.render("{{#goodbyes}}{{text}}! {{/goodbyes}}cruel {{world}}!", ctx) ==
            "goodbye! Goodbye! GOODBYE! cruel world!");

        // Arrays ignore the contents when empty
        BOOST_TEST(
            hbs.render("{{#goodbyes}}{{text}}! {{/goodbyes}}cruel {{world}}!", emptyCtx) ==
            "cruel world!");
    }

    // array without data
    {
        BOOST_TEST(
            hbs.render("{{#goodbyes}}{{text}}{{/goodbyes}} {{#goodbyes}}{{text}}{{/goodbyes}}", ctx) ==
            "goodbyeGoodbyeGOODBYE goodbyeGoodbyeGOODBYE");
    }

    // array with @index
    {
        // The @index variable is used
        BOOST_TEST(
            hbs.render("{{#goodbyes}}{{@index}}. {{text}}! {{/goodbyes}}cruel {{world}}!", ctx) ==
            "0. goodbye! 1. Goodbye! 2. GOODBYE! cruel world!");
    }

    // empty block
    {
        // Arrays iterate over the contents when not empty
        BOOST_TEST(
            hbs.render("{{#goodbyes}}{{/goodbyes}}cruel {{world}}!", ctx) ==
            "cruel world!");

        // Arrays ignore the contents when empty
        BOOST_TEST(
            hbs.render("{{#goodbyes}}{{/goodbyes}}cruel {{world}}!", emptyCtx) ==
            "cruel world!");
    }

    // block with complex lookup
    {
        // Templates can access variables in contexts up the stack with relative path syntax
        BOOST_TEST(
            hbs.render("{{#goodbyes}}{{text}} cruel {{../name}}! {{/goodbyes}}", ctx) ==
            "goodbye cruel Alan! Goodbye cruel Alan! GOODBYE cruel Alan! ");
    }

    // multiple blocks with complex lookup
    {
        BOOST_TEST(
            hbs.render("{{#goodbyes}}{{../name}}{{../name}}{{/goodbyes}}", ctx) ==
            "AlanAlanAlanAlanAlanAlan");
    }

    // block with complex lookup using nested context
    {
        // In this test, we pass in the context so that the block is
        // evaluated and the error is thrown in runtime.
        BOOST_TEST_THROW_WITH(
            hbs.render("{{#goodbyes}}{{text}} cruel {{foo/../name}}! {{/goodbyes}}", ctx),
            HandlebarsError, "Invalid path: foo/.. - 1:30");
    }

    // block with deep nested complex lookup
    {
        // { omg: 'OMG!', outer: [{ sibling: 'sad', inner: [{ text: 'goodbye' }] }] }
        dom::Object ctx2;
        dom::Array outer;
        dom::Object outer1;
        dom::Array inner;
        dom::Object inner1;
        inner1.set("text", "goodbye");
        inner.emplace_back(inner1);
        outer1.set("sibling", "sad");
        outer1.set("inner", inner);
        outer.emplace_back(outer1);
        ctx2.set("omg", "OMG!");
        ctx2.set("outer", outer);

        BOOST_TEST(
            hbs.render("{{#outer}}Goodbye {{#inner}}cruel {{../sibling}} {{../../omg}}{{/inner}}{{/outer}}", ctx2) ==
            "Goodbye cruel sad OMG!");
    }

    // works with cached blocks
    {
        // { person: [ { first: 'Alan', last: 'Johnson' }, { first: 'Alan', last: 'Johnson' } ] }
        dom::Object ctx2;
        dom::Array person;
        dom::Object person1;
        person1.set("first", "Alan");
        person1.set("last", "Johnson");
        person.emplace_back(person1);
        person.emplace_back(person1);
        ctx2.set("person", person);

        HandlebarsOptions opt;
        opt.data = false;
        BOOST_TEST(
            hbs.render("{{#each person}}{{#with .}}{{first}} {{last}}{{/with}}{{/each}}", ctx2, opt) ==
            "Alan JohnsonAlan Johnson");
    }
}

void
block_inverted_sections()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/blocks.js
    Handlebars hbs;

    // inverted sections with unset value
    {
        // Inverted section rendered when value isn't set.
        BOOST_TEST(
            hbs.render("{{#goodbyes}}{{this}}{{/goodbyes}}{{^goodbyes}}Right On!{{/goodbyes}}") ==
            "Right On!");
    }

    // inverted section with false value
    {
        // Inverted section rendered when value is false.
        dom::Object ctx;
        ctx.set("goodbyes", false);
        BOOST_TEST(
            hbs.render("{{#goodbyes}}{{this}}{{/goodbyes}}{{^goodbyes}}Right On!{{/goodbyes}}", ctx) ==
            "Right On!");

    }

    // inverted section with empty set
    {
        // Inverted section rendered when value is empty set.
        dom::Object ctx;
        dom::Array goodbyes;
        ctx.set("goodbyes", goodbyes);
        BOOST_TEST(
            hbs.render("{{#goodbyes}}{{this}}{{/goodbyes}}{{^goodbyes}}Right On!{{/goodbyes}}", ctx) ==
            "Right On!");
    }

    // block inverted sections
    {
        dom::Object ctx;
        ctx.set("none", "No people");
        BOOST_TEST(
            hbs.render("{{#people}}{{name}}{{^}}{{none}}{{/people}}", ctx) ==
            "No people");
    }

    // chained inverted sections
    {
        dom::Object ctx;
        ctx.set("none", "No people");
        BOOST_TEST(
            hbs.render("{{#people}}{{name}}{{else if none}}{{none}}{{/people}}", ctx) ==
            "No people");
        BOOST_TEST(
            hbs.render("{{#people}}{{name}}{{else if nothere}}fail{{else unless nothere}}{{none}}{{/people}}", ctx) ==
            "No people");
        BOOST_TEST(
            hbs.render("{{#people}}{{name}}{{else if none}}{{none}}{{else}}fail{{/people}}", ctx) ==
            "No people");
    }

    // chained inverted sections with mismatch
    {
        BOOST_TEST_THROW_WITH(
            hbs.render("{{#people}}{{name}}{{else if none}}{{none}}{{/if}}"),
            HandlebarsError, "people doesn't match if - 1:3");
    }

    // block inverted sections with empty arrays
    {
        // { none: 'No people', people: [] }
        dom::Object ctx;
        ctx.set("none", "No people");
        dom::Array people;
        ctx.set("people", people);
        BOOST_TEST(
            hbs.render("{{#people}}{{name}}{{^}}{{none}}{{/people}}", ctx) ==
            "No people");
    }
}

void
block_standalone_sections()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/blocks.js
    Handlebars hbs;

    // block standalone else sections
    {
        dom::Object ctx;
        ctx.set("none", "No people");
        BOOST_TEST(
            hbs.render("{{#people}}\n{{name}}\n{{^}}\n{{none}}\n{{/people}}\n", ctx) ==
            "No people\n");
        BOOST_TEST(
            hbs.render("{{#none}}\n{{.}}\n{{^}}\n{{none}}\n{{/none}}\n", ctx) ==
            "No people\n");
        BOOST_TEST(
            hbs.render("{{#people}}\n{{name}}\n{{^}}\n{{none}}\n{{/people}}\n", ctx) ==
            "No people\n");
        BOOST_TEST(
            hbs.render("  {{#people}}\n{{name}}\n{{^}}\n{{none}}\n{{/people}}\n", ctx) ==
            "No people\n");

    }

    // block standalone else sections can be disabled
    {
        dom::Object ctx;
        ctx.set("none", "No people");
        HandlebarsOptions opt;
        opt.ignoreStandalone = true;
        BOOST_TEST(
            hbs.render("{{#people}}\n{{name}}\n{{^}}\n{{none}}\n{{/people}}\n", ctx, opt) ==
            "\nNo people\n\n");
        BOOST_TEST(
            hbs.render("{{#none}}\n{{.}}\n{{^}}\nFail\n{{/none}}\n", ctx, opt) ==
            "\nNo people\n\n");
    }

    // block standalone chained else sections
    {
        dom::Object ctx;
        ctx.set("none", "No people");
        BOOST_TEST(
            hbs.render("{{#people}}\n{{name}}\n{{else if none}}\n{{none}}\n{{/people}}\n", ctx) ==
            "No people\n");
        BOOST_TEST(
            hbs.render("{{#people}}\n{{name}}\n{{else if none}}\n{{none}}\n{{^}}\n{{/people}}\n", ctx) ==
            "No people\n");
    }

    // should handle nesting
    {
        // { data: [1, 3, 5] }
        dom::Object ctx;
        dom::Array data;
        data.emplace_back(1);
        data.emplace_back(3);
        data.emplace_back(5);
        ctx.set("data", data);
        BOOST_TEST(
            hbs.render("{{#data}}\n{{#if true}}\n{{.}}\n{{/if}}\n{{/data}}\nOK.", ctx) ==
            "1\n3\n5\nOK.");
    }
}

void
block_compat_mode()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/blocks.js
    Handlebars hbs;
    HandlebarsOptions compat;
    compat.compat = true;

    // block with deep recursive lookup lookup
    {
         // { omg: 'OMG!', outer: [{ inner: [{ text: 'goodbye' }] }] }
        dom::Object ctx;
        ctx.set("omg", "OMG!");
        dom::Array outer;
        dom::Object outer1;
        dom::Array inner;
        dom::Object inner1;
        inner1.set("text", "goodbye");
        inner.emplace_back(inner1);
        outer1.set("inner", inner);
        outer.emplace_back(outer1);
        ctx.set("outer", outer);
        BOOST_TEST(
            hbs.render("{{#outer}}Goodbye {{#inner}}cruel {{omg}}{{/inner}}{{/outer}}", ctx, compat) ==
            "Goodbye cruel OMG!");
    }

    // block with deep recursive pathed lookup
    {
        // { omg: { yes: 'OMG!' }, outer: [{ inner: [{ yes: 'no', text: 'goodbye' }] }] }
        dom::Object ctx;
        dom::Object omg;
        omg.set("yes", "OMG!");
        ctx.set("omg", omg);
        dom::Array outer;
        dom::Object outer1;
        dom::Array inner;
        dom::Object inner1;
        inner1.set("yes", "no");
        inner1.set("text", "goodbye");
        inner.emplace_back(inner1);
        outer1.set("inner", inner);
        outer.emplace_back(outer1);
        ctx.set("outer", outer);
        BOOST_TEST(
            hbs.render("{{#outer}}Goodbye {{#inner}}cruel {{omg.yes}}{{/inner}}{{/outer}}", ctx, compat) ==
            "Goodbye cruel OMG!");
    }

    // block with missed recursive lookup
    {
        // { omg: { no: 'OMG!' }, outer: [{ inner: [{ yes: 'no', text: 'goodbye' }] }] }
        dom::Object ctx;
        dom::Object omg;
        omg.set("no", "OMG!");
        ctx.set("omg", omg);
        dom::Array outer;
        dom::Object outer1;
        dom::Array inner;
        dom::Object inner1;
        inner1.set("yes", "no");
        inner1.set("text", "goodbye");
        inner.emplace_back(inner1);
        outer1.set("inner", inner);
        outer.emplace_back(outer1);
        ctx.set("outer", outer);
        BOOST_TEST(
            hbs.render("{{#outer}}Goodbye {{#inner}}cruel {{omg.yes}}{{/inner}}{{/outer}}", ctx, compat) ==
            "Goodbye cruel ");
    }
}

void
block_decorators()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/blocks.js
    // https://handlebarsjs.com/api-reference/runtime.html#handlebars-registerdecorator-name-helper-deprecated
    // Custom decorators are deprecated and may vanish in the next major version
    // of Handlebars. They expose a too large part of the private internal API
    // which is difficult to port to other languages and makes to code harder
    // to maintain.
}

void
subexpressions()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/subexpressions.js
    Handlebars hbs;

    // arg-less helper
    {
        hbs.registerHelper("foo", [](dom::Array const& args) {
            std::string res(args[0].getString());
            res += args[0].getString();
            return res;
        });
        hbs.registerHelper("bar", []() {
            return "LOL";
        });
        BOOST_TEST(hbs.render("{{foo (bar)}}!") == "LOLLOL!");
    }

    // helper with args
    {
        hbs.registerHelper("blog", [](dom::Array const& args) {
            std::string res = "val is ";
            res += toString(args[0]);
            return res;
        });
        hbs.registerHelper("equal", [](dom::Array const& args) {
            return dom::JSON_stringify(args[0]) == dom::JSON_stringify(args[1]);
        });
        dom::Object ctx;
        ctx.set("bar", "LOL");
        BOOST_TEST(hbs.render("{{blog (equal a b)}}", ctx) == "val is true");
    }

    // mixed paths and helpers
    {
        dom::Object ctx;
        ctx.set("bar", "LOL");
        dom::Object baz;
        baz.set("bat", "foo!");
        baz.set("bar", "bar!");
        ctx.set("baz", baz);
        hbs.registerHelper("blog", [](dom::Array const& args) {
            std::string res = "val is ";
            res += toString(args[0]);
            res += ", ";
            res += toString(args[1]);
            res += " and ";
            res += toString(args[2]);
            return res;
        });
        hbs.registerHelper("equal", [](dom::Array const& args) {
            return dom::JSON_stringify(args[0]) == dom::JSON_stringify(args[1]);
        });
        BOOST_TEST(
            hbs.render("{{blog baz.bat (equal a b) baz.bar}}", ctx) ==
            "val is foo!, true and bar!");
    }

    // supports much nesting
    {
        dom::Object ctx;
        ctx.set("bar", "LOL");
        hbs.registerHelper("blog", [](dom::Array const& args) {
            std::string res = "val is ";
            res += toString(args[0]);
            return res;
        });
        hbs.registerHelper("equal", [](dom::Array const& args) {
            return dom::JSON_stringify(args[0]) == dom::JSON_stringify(args[1]);
        });
        BOOST_TEST(
            hbs.render("{{blog (equal (equal true true) true)}}", ctx) == "val is true");
    }

    // GH-800 : Complex subexpressions
    {
        // { a: 'a', b: 'b', c: { c: 'c' }, d: 'd', e: { e: 'e' } };
        dom::Object context;
        context.set("a", "a");
        context.set("b", "b");
        dom::Object c;
        c.set("c", "c");
        context.set("c", c);
        context.set("d", "d");
        dom::Object e;
        e.set("e", "e");
        context.set("e", e);

        hbs.registerHelper("dash", [](dom::Array const& args) {
            std::string res = toString(args[0]);
            res += "-";
            res += toString(args[1]);
            return res;
        });
        hbs.registerHelper("concat", [](dom::Array const& args) {
            std::string res = toString(args[0]);
            res += toString(args[1]);
            return res;
        });
        BOOST_TEST(
            hbs.render("{{dash 'abc' (concat a b)}}", context) == "abc-ab");
        BOOST_TEST(
            hbs.render("{{dash d (concat a b)}}", context) == "d-ab");
        BOOST_TEST(
            hbs.render("{{dash c.c (concat a b)}}", context) == "c-ab");
        BOOST_TEST(
            hbs.render("{{dash (concat a b) c.c}}", context) == "ab-c");
        BOOST_TEST(
            hbs.render("{{dash (concat a e.e) c.c}}", context) == "ae-c");
    }

    // provides each nested helper invocation its own options hash
    {
        HandlebarsCallback const* lastOptions = nullptr;
        hbs.registerHelper("equal", [&](
            dom::Array const& args, HandlebarsCallback const& options)
        {
            if (&options == lastOptions)
            {
                return false;
            }
            lastOptions = &options;
            return dom::JSON_stringify(args[0]) == dom::JSON_stringify(args[1]);
        });
        BOOST_TEST(
            hbs.render("{{equal (equal true true) true}}") == "true");
    }

    // with hashes
    {
        dom::Object ctx;
        ctx.set("bar", "LOL");
        hbs.registerHelper("blog", [](dom::Array const& args) {
            std::string res = "val is ";
            res += toString(args[0]);
            return res;
        });
        hbs.registerHelper("equal", [](dom::Array const& args) {
            return dom::JSON_stringify(args[0]) == dom::JSON_stringify(args[1]);
        });
        BOOST_TEST(
            hbs.render("{{blog (equal (equal true true) true fun='yes')}}", ctx) ==
            "val is true");
    }

    // as hashes
    {
        hbs.registerHelper("blog", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            std::string res = "val is ";
            res += toString(cb.hash().find("fun"));
            return res;
        });
        hbs.registerHelper("equal", [](dom::Array const& args) {
            return dom::JSON_stringify(args[0]) == dom::JSON_stringify(args[1]);
        });
        BOOST_TEST(
            hbs.render("{{blog fun=(equal (blog fun=1) 'val is 1')}}") ==
            "val is true");
    }

    // multiple subexpressions in a hash
    {
        hbs.registerHelper("input", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            auto hash = cb.hash();
            std::string ariaLabel = escapeExpression(hash.find("aria-label").getString());
            std::string placeholder = escapeExpression(hash.find("placeholder").getString());
            std::string res = "<input aria-label=\"";
            res += ariaLabel;
            res += "\" placeholder=\"";
            res += placeholder;
            res += "\" />";
            return safeString(res);
        });
        hbs.registerHelper("t", [](dom::Array const& args) {
            return safeString(args[0].getString());
        });
        BOOST_TEST(
            hbs.render("{{input aria-label=(t \"Name\") placeholder=(t \"Example User\")}}") ==
            "<input aria-label=\"Name\" placeholder=\"Example User\" />");
    }

    // multiple subexpressions in a hash with context
    {
        dom::Object ctx;
        dom::Object item;
        item.set("field", "Name");
        item.set("placeholder", "Example User");
        ctx.set("item", item);
        hbs.registerHelper("input", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            auto hash = cb.hash();
            std::string ariaLabel = escapeExpression(hash.find("aria-label").getString());
            std::string placeholder = escapeExpression(hash.find("placeholder").getString());
            std::string res = "<input aria-label=\"";
            res += ariaLabel;
            res += "\" placeholder=\"";
            res += placeholder;
            res += "\" />";
            return safeString(res);
        });
        hbs.registerHelper("t", [](dom::Array const& args) {
            return safeString(args[0].getString());
        });
        BOOST_TEST(
            hbs.render("{{input aria-label=(t item.field) placeholder=(t item.placeholder)}}", ctx) ==
            "<input aria-label=\"Name\" placeholder=\"Example User\" />");
    }

    // in string params mode
    {
        dom::Object ctx;
        ctx.set("foo", "foo");
        ctx.set("yeah", "yeah");
        hbs.registerHelper("snog", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            BOOST_TEST(args[0].getString() == "foo");
            // string params for outer helper processed correctly
            BOOST_TEST(args.size() == 2u);
            std::string res(args[0].getString());
            res += args[1].getString();
            return safeString(res);
        });
        hbs.registerHelper("blorg", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            // string params for inner helper processed correctly
            BOOST_TEST(args.size() == 1u);
            return args[0];
        });
        BOOST_TEST(
            hbs.render("{{snog (blorg foo x=y) yeah a=b}}", ctx) ==
            "fooyeah");
    }

    // as hashes in string params mode
    {
        hbs.registerHelper("blog", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            std::string res = "val is ";
            res += toString(cb.hash().find("fun"));
            return res;
        });
        hbs.registerHelper("bork", [](
            dom::Array const& args, HandlebarsCallback const& cb) {
            return "BORK";
        });
        BOOST_TEST(hbs.render("{{blog fun=(bork)}}") == "val is BORK");
    }

    // subexpression functions on the context
    {
        hbs.registerHelper("bar", []() {
            return "LOL";
        });
        hbs.registerHelper("foo", [](dom::Array const& args) {
            dom::Value val = args[0];
            std::string res(val.getString());
            res += val.getString();
            return res;
        });
        BOOST_TEST(hbs.render("{{foo (bar)}}!") == "LOLLOL!");
    }

    // subexpressions can't just be property lookups
    {
        dom::Object ctx;
        ctx.set("bar", "LOL");
        hbs.unregisterHelper("bar");
        hbs.registerHelper("foo", [](dom::Array const& args) {
            dom::Value val = args[0];
            std::string res(val.getString());
            res += val.getString();
            return res;
        });
        BOOST_TEST_THROW_WITH(
            hbs.render("{{foo (bar)}}!"), HandlebarsError, "bar is not a function - 1:7");
    }
}

void
builtin_if()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/builtins.js
    Handlebars hbs;

    // if
    {
        std::string string = "{{#if goodbye}}GOODBYE {{/if}}cruel {{world}}!";

        // if with boolean argument shows the contents when true
        dom::Object ctx;
        ctx.set("goodbye", true);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "GOODBYE cruel world!");

        // if with string argument shows the contents
        ctx.set("goodbye", "dummy");
        BOOST_TEST(hbs.render(string, ctx) == "GOODBYE cruel world!");

        // if with boolean argument does not show the contents when false
        ctx.set("goodbye", false);
        BOOST_TEST(hbs.render(string, ctx) == "cruel world!");

        // if with undefined does not show the contents
        ctx = dom::Object();
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "cruel world!");

        // if with non-empty array shows the contents
        ctx = dom::Object();
        dom::Array fooArray;
        fooArray.emplace_back("foo");
        ctx.set("goodbye", fooArray);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "GOODBYE cruel world!");

        // if with empty array does not show the contents
        dom::Array emptyArray;
        ctx.set("goodbye", emptyArray);
        BOOST_TEST(hbs.render(string, ctx) == "cruel world!");

        // if with zero does not show the contents
        ctx.set("goodbye", 0);
        BOOST_TEST(hbs.render(string, ctx) == "cruel world!");

        // if with zero does show the contents
        BOOST_TEST(
            hbs.render("{{#if goodbye includeZero=true}}GOODBYE {{/if}}cruel {{world}}!", ctx) ==
            "GOODBYE cruel world!");
    }

    // if with function argument
    {
        std::string string = "{{#if goodbye}}GOODBYE {{/if}}cruel {{world}}!";

        // if with function shows the contents when function returns true
        dom::Object ctx;
        ctx.set("goodbye", dom::makeInvocable([]() { return true; }));
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "GOODBYE cruel world!");

        // if with function shows the contents when function returns string
        ctx.set("goodbye", dom::makeInvocable([](dom::Object const& ctx) { return ctx.find("world"); }));
        BOOST_TEST(hbs.render(string, ctx) == "GOODBYE cruel world!");

        // if with function does not show the contents when returns false
        ctx.set("goodbye", dom::makeInvocable([]() { return false; }));
        BOOST_TEST(hbs.render(string, ctx) == "cruel world!");

        // if with function does not show the contents when returns undefined
        ctx.set("goodbye", dom::makeInvocable([](dom::Object const& ctx) { return ctx.find("foo"); }));
        BOOST_TEST(hbs.render(string, ctx) == "cruel world!");
    }

    // should not change the depth list
    {
        std::string string = "{{#with foo}}{{#if goodbye}}GOODBYE cruel {{../world}}!{{/if}}{{/with}}";
        // { foo: { goodbye: true }, world: 'world' }
        dom::Object ctx;
        dom::Object foo;
        foo.set("goodbye", true);
        ctx.set("foo", foo);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "GOODBYE cruel world!");
    }
}

void
builtin_with()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/builtins.js
    Handlebars hbs;

    // with
    {
        std::string string = "{{#with person}}{{first}} {{last}}{{/with}}";
        dom::Object ctx;
        dom::Object person;
        person.set("first", "Alan");
        person.set("last", "Johnson");
        ctx.set("person", person);
        BOOST_TEST(hbs.render(string, ctx) == "Alan Johnson");
    }

    // with helper with function argument
    {
        std::string string = "{{#with person}}{{first}} {{last}}{{/with}}";
        dom::Object ctx;
        ctx.set("person", dom::makeInvocable([]() {
            dom::Object person;
            person.set("first", "Alan");
            person.set("last", "Johnson");
            return dom::Value(person);
        }));
        BOOST_TEST(hbs.render(string, ctx) == "Alan Johnson");
    }

    // with helper with else
    {
        std::string string = "{{#with person}}Person is present{{else}}Person is not present{{/with}}";
        dom::Object ctx;
        BOOST_TEST(hbs.render(string, ctx) == "Person is not present");
    }

    // with provides block parameter
    {
        std::string string = "{{#with person as |foo|}}{{foo.first}} {{last}}{{/with}}";
        dom::Object ctx;
        dom::Object person;
        person.set("first", "Alan");
        person.set("last", "Johnson");
        ctx.set("person", person);
        BOOST_TEST(hbs.render(string, ctx) == "Alan Johnson");
    }

    // works when data is disabled
    {
        std::string string = "{{#with person as |foo|}}{{foo.first}} {{last}}{{/with}}";
        dom::Object ctx;
        dom::Object person;
        person.set("first", "Alan");
        person.set("last", "Johnson");
        ctx.set("person", person);
        HandlebarsOptions options;
        options.data = false;
        BOOST_TEST(hbs.render(string, ctx, options) == "Alan Johnson");
    }
}

void
builtin_each()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/builtins.js
    Handlebars hbs;

    // each
    {
        std::string string = "{{#each goodbyes}}{{text}}! {{/each}}cruel {{world}}!";

        // each with array argument iterates over the contents when not empty
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        dom::Object goodbye3;
        goodbye3.set("text", "GOODBYE");
        goodbyes.emplace_back(goodbye1);
        goodbyes.emplace_back(goodbye2);
        goodbyes.emplace_back(goodbye3);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "goodbye! Goodbye! GOODBYE! cruel world!");

        // each with array argument ignores the contents when empty
        ctx.set("goodbyes", dom::Array());
        BOOST_TEST(hbs.render(string, ctx) == "cruel world!");
    }

    // each without data
    {
        std::string string = "{{#each goodbyes}}{{text}}! {{/each}}cruel {{world}}!";
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        dom::Object goodbye3;
        goodbye3.set("text", "GOODBYE");
        goodbyes.emplace_back(goodbye1);
        goodbyes.emplace_back(goodbye2);
        goodbyes.emplace_back(goodbye3);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        HandlebarsOptions options;
        options.data = false;
        BOOST_TEST(hbs.render(string, ctx, options) == "goodbye! Goodbye! GOODBYE! cruel world!");

        std::string string2 = "{{#each .}}{{.}}{{/each}}";
        ctx.set("goodbyes", "cruel");
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string2, ctx, options) == "cruelworld");
    }

    // each without context
    {
        std::string string = "{{#each goodbyes}}{{text}}! {{/each}}cruel {{world}}!";
        dom::Value ctx;
        BOOST_TEST(hbs.render(string, ctx) == "cruel !");
    }

    // each with an object and @key
    {
        std::string string = "{{#each goodbyes}}{{@key}}. {{text}}! {{/each}}cruel {{world}}!";

        dom::Value Clazz = dom::makeInvocable([]() {
            dom::Object obj;
            dom::Object goodbye1;
            goodbye1.set("text", "goodbye");
            obj.set("<b>#1</b>", goodbye1);
            dom::Object goodbye2;
            goodbye2.set("text", "GOODBYE");
            obj.set("2", goodbye2);
            return dom::Value(obj);
        });
        dom::Object hash;
        hash.set("goodbyes", Clazz);
        hash.set("world", "world");
        BOOST_TEST(hbs.render(string, hash) == "&lt;b&gt;#1&lt;/b&gt;. goodbye! 2. GOODBYE! cruel world!");

        dom::Object ctx;
        ctx.set("goodbyes", dom::Object());
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "cruel world!");
    }

    // each with @index
    {
        std::string string = "{{#each goodbyes}}{{@index}}. {{text}}! {{/each}}cruel {{world}}!";
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        dom::Object goodbye3;
        goodbye3.set("text", "GOODBYE");
        goodbyes.emplace_back(goodbye1);
        goodbyes.emplace_back(goodbye2);
        goodbyes.emplace_back(goodbye3);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "0. goodbye! 1. Goodbye! 2. GOODBYE! cruel world!");
    }

    // each with nested @index
    {
        std::string string = "{{#each goodbyes}}{{@index}}. {{text}}! {{#each ../goodbyes}}{{@index}} {{/each}}After {{@index}} {{/each}}{{@index}}cruel {{world}}!";
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        dom::Object goodbye3;
        goodbye3.set("text", "GOODBYE");
        goodbyes.emplace_back(goodbye1);
        goodbyes.emplace_back(goodbye2);
        goodbyes.emplace_back(goodbye3);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "0. goodbye! 0 1 2 After 0 1. Goodbye! 0 1 2 After 1 2. GOODBYE! 0 1 2 After 2 cruel world!");
    }

    // each with block params
    {
        std::string string = "{{#each goodbyes as |value index|}}{{index}}. {{value.text}}! {{#each ../goodbyes as |childValue childIndex|}} {{index}} {{childIndex}}{{/each}} After {{index}} {{/each}}{{index}}cruel {{world}}!";
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        goodbyes.emplace_back(goodbye1);
        goodbyes.emplace_back(goodbye2);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "0. goodbye!  0 0 0 1 After 0 1. Goodbye!  1 0 1 1 After 1 cruel world!");
    }

    // each with block params and strict compilation
    {
        std::string string = "{{#each goodbyes as |value index|}}{{index}}. {{value.text}}!{{/each}}";
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        goodbyes.emplace_back(goodbye1);
        goodbyes.emplace_back(goodbye2);
        ctx.set("goodbyes", goodbyes);
        HandlebarsOptions options;
        options.strict = true;
        BOOST_TEST(hbs.render(string, ctx, options) == "0. goodbye!1. Goodbye!");
    }

    // each object with @index
    {
        std::string string = "{{#each goodbyes}}{{@index}}. {{text}}! {{/each}}cruel {{world}}!";
        dom::Object ctx;
        dom::Object goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        dom::Object goodbye3;
        goodbye3.set("text", "GOODBYE");
        goodbyes.set("a", goodbye1);
        goodbyes.set("b", goodbye2);
        goodbyes.set("c", goodbye3);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "0. goodbye! 1. Goodbye! 2. GOODBYE! cruel world!");
    }

    // each with @first
    {
        std::string string = "{{#each goodbyes}}{{#if @first}}{{text}}! {{/if}}{{/each}}cruel {{world}}!";
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        dom::Object goodbye3;
        goodbye3.set("text", "GOODBYE");
        goodbyes.emplace_back(goodbye1);
        goodbyes.emplace_back(goodbye2);
        goodbyes.emplace_back(goodbye3);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "goodbye! cruel world!");
    }

    // each with nested @first
    {
        std::string string = "{{#each goodbyes}}({{#if @first}}{{text}}! {{/if}}{{#each ../goodbyes}}{{#if @first}}{{text}}!{{/if}}{{/each}}{{#if @first}} {{text}}!{{/if}}) {{/each}}cruel {{world}}!";
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        dom::Object goodbye3;
        goodbye3.set("text", "GOODBYE");
        goodbyes.emplace_back(goodbye1);
        goodbyes.emplace_back(goodbye2);
        goodbyes.emplace_back(goodbye3);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "(goodbye! goodbye! goodbye!) (goodbye!) (goodbye!) cruel world!");
    }

    // each object with @first
    {
        std::string string = "{{#each goodbyes}}{{#if @first}}{{text}}! {{/if}}{{/each}}cruel {{world}}!";
        dom::Object ctx;
        dom::Object goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        goodbyes.set("foo", goodbye1);
        goodbyes.set("bar", goodbye2);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "goodbye! cruel world!");
    }

    // each with @last
    {
        std::string string = "{{#each goodbyes}}{{#if @last}}{{text}}! {{/if}}{{/each}}cruel {{world}}!";
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        dom::Object goodbye3;
        goodbye3.set("text", "GOODBYE");
        goodbyes.emplace_back(goodbye1);
        goodbyes.emplace_back(goodbye2);
        goodbyes.emplace_back(goodbye3);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "GOODBYE! cruel world!");
    }

    // each object with @last
    {
        std::string string = "{{#each goodbyes}}{{#if @last}}{{text}}! {{/if}}{{/each}}cruel {{world}}!";
        dom::Object ctx;
        dom::Object goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        goodbyes.set("foo", goodbye1);
        goodbyes.set("bar", goodbye2);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "Goodbye! cruel world!");
    }

    // each with nested @last
    {
        std::string string = "{{#each goodbyes}}({{#if @last}}{{text}}! {{/if}}{{#each ../goodbyes}}{{#if @last}}{{text}}!{{/if}}{{/each}}{{#if @last}} {{text}}!{{/if}}) {{/each}}cruel {{world}}!";
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        dom::Object goodbye3;
        goodbye3.set("text", "GOODBYE");
        goodbyes.emplace_back(goodbye1);
        goodbyes.emplace_back(goodbye2);
        goodbyes.emplace_back(goodbye3);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "(GOODBYE!) (GOODBYE!) (GOODBYE! GOODBYE! GOODBYE!) cruel world!");
    }

    // each with function argument
    {
        std::string string = "{{#each goodbyes}}{{text}}! {{/each}}cruel {{world}}!";
        dom::Object ctx;
        ctx.set("goodbyes", dom::makeInvocable([]() {
            dom::Array goodbyes;
            dom::Object goodbye1;
            goodbye1.set("text", "goodbye");
            dom::Object goodbye2;
            goodbye2.set("text", "Goodbye");
            dom::Object goodbye3;
            goodbye3.set("text", "GOODBYE");
            goodbyes.emplace_back(goodbye1);
            goodbyes.emplace_back(goodbye2);
            goodbyes.emplace_back(goodbye3);
            return dom::Value(goodbyes);
        }));
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "goodbye! Goodbye! GOODBYE! cruel world!");

        ctx.set("goodbyes", dom::makeInvocable([]() {
            dom::Array goodbyes;
            return dom::Value(goodbyes);
        }));
        BOOST_TEST(hbs.render(string, ctx) == "cruel world!");
    }

    // each object when last key is an empty string
    {
        std::string string = "{{#each goodbyes}}{{@index}}. {{text}}! {{/each}}cruel {{world}}!";
        dom::Object ctx;
        dom::Object goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "goodbye");
        dom::Object goodbye2;
        goodbye2.set("text", "Goodbye");
        dom::Object goodbye3;
        goodbye3.set("text", "GOODBYE");
        goodbyes.set("a", goodbye1);
        goodbyes.set("b", goodbye2);
        goodbyes.set("", goodbye3);
        ctx.set("goodbyes", goodbyes);
        ctx.set("world", "world");
        BOOST_TEST(hbs.render(string, ctx) == "0. goodbye! 1. Goodbye! 2. GOODBYE! cruel world!");
    }

    // data passed to helpers
    {
        std::string string = "{{#each letters}}{{this}}{{detectDataInsideEach}}{{/each}}";
        dom::Object ctx;
        dom::Array letters;
        letters.emplace_back("a");
        letters.emplace_back("b");
        letters.emplace_back("c");
        ctx.set("letters", letters);
        hbs.registerHelper("detectDataInsideEach", [](
            dom::Array const& args, HandlebarsCallback const& options)
        {
            return options.data().find("exclaim");
        });
        HandlebarsOptions options;
        dom::Object data;
        data.set("exclaim", "!");
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "a!b!c!");
    }

    // each on implicit context
    {
        std::string string = "{{#each}}{{text}}! {{/each}}cruel world!";
        BOOST_TEST_THROW_WITH(
            hbs.render(string, dom::Object()),
            HandlebarsError, "Must pass iterator to #each");
    }
}

void
builtin_log()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/builtins.js
    Handlebars hbs;

    dom::Value levelArg;
    dom::Array logArgs;
    dom::Value logArg;

    hbs.registerLogger([&](dom::Value level, dom::Array const& args) {
        levelArg = level;
        logArgs = args;
        logArg = nullptr;
        if (!args.empty())
        {
            logArg = args[0];
        }
    });

    // should call logger at default level
    {
        std::string string = "{{log blah}}";
        dom::Object ctx;
        ctx.set("blah", "whee");
        BOOST_TEST(hbs.render(string, ctx) == "");
        BOOST_TEST(levelArg.isInteger());
        BOOST_TEST(levelArg.getInteger() == 1);
        BOOST_TEST(logArg.isString());
        BOOST_TEST(logArg.getString() == "whee");
    }

    // should call logger at data level
    {
        std::string string = "{{log blah}}";
        dom::Object ctx;
        ctx.set("blah", "whee");
        HandlebarsOptions options;
        dom::Object data;
        data.set("level", "03");
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "");
        BOOST_TEST(levelArg.isString());
        BOOST_TEST(levelArg.getString() == "03");
        BOOST_TEST(logArg.isString());
        BOOST_TEST(logArg.getString() == "whee");
    }

    // should handle string log levels
    {
        dom::Object ctx;
        ctx.set("blah", "whee");
        HandlebarsOptions options;
        dom::Object data;
        data.set("level", "error");
        options.data = data;
        BOOST_TEST(hbs.render("{{log blah}}", ctx, options) == "");
        BOOST_TEST(levelArg.isString());
        BOOST_TEST(levelArg.getString() == "error");
        BOOST_TEST(logArg.isString());
        BOOST_TEST(logArg.getString() == "whee");
    }

    // should handle hash log levels
    {
        dom::Object ctx;
        ctx.set("blah", "whee");
        BOOST_TEST(hbs.render("{{log blah level=\"debug\"}}", ctx) == "");
        BOOST_TEST(levelArg.isString());
        BOOST_TEST(levelArg.getString() == "debug");
        BOOST_TEST(logArg.isString());
        BOOST_TEST(logArg.getString() == "whee");
    }

    // should pass multiple log arguments
    {
        dom::Object ctx;
        ctx.set("blah", "whee");
        BOOST_TEST(hbs.render("{{log blah \"foo\" 1}}", ctx) == "");
        BOOST_TEST(levelArg.isInteger());
        BOOST_TEST(levelArg.getInteger() == 1);
        BOOST_TEST(logArgs.size() == 3u);
        BOOST_TEST(logArgs[0].isString());
        BOOST_TEST(logArgs[0].getString() == "whee");
        BOOST_TEST(logArgs[1].isString());
        BOOST_TEST(logArgs[1].getString() == "foo");
        BOOST_TEST(logArgs[2].isInteger());
        BOOST_TEST(logArgs[2].getInteger() == 1);
    }

    // should pass zero log arguments
    {
        dom::Object ctx;
        ctx.set("blah", "whee");
        BOOST_TEST(hbs.render("{{log}}", ctx) == "");
        BOOST_TEST(levelArg.isInteger());
        BOOST_TEST(levelArg.getInteger() == 1);
        BOOST_TEST(logArgs.empty());
    }
}

void
builtin_lookup()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/builtins.js
    Handlebars hbs;

    // should look up arbitrary content
    {
        std::string string = "{{#each goodbyes}}{{lookup ../data .}}{{/each}}";
        dom::Object ctx;
        dom::Array goodbyes;
        goodbyes.emplace_back(0);
        goodbyes.emplace_back(1);
        ctx.set("goodbyes", goodbyes);
        dom::Array data;
        data.emplace_back("foo");
        data.emplace_back("bar");
        ctx.set("data", data);
        BOOST_TEST(hbs.render(string, ctx) == "foobar");
    }

    // should not fail on undefined value
    {
        std::string string = "{{#each goodbyes}}{{lookup ../bar .}}{{/each}}";
        dom::Object ctx;
        dom::Array goodbyes;
        goodbyes.emplace_back(0);
        goodbyes.emplace_back(1);
        ctx.set("goodbyes", goodbyes);
        dom::Array data;
        data.emplace_back("foo");
        data.emplace_back("bar");
        ctx.set("data", data);
        BOOST_TEST(hbs.render(string, ctx) == "");
    }
}

void
data() const
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/data.js
    Handlebars hbs;

    // passing in data to a compiled function that expects data - works with helpers
    {
        std::string string = "{{hello}}";
        dom::Object ctx;
        ctx.set("noun", "cat");
        hbs.registerHelper("hello", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string a(options.data().find("adjective").getString());
            std::string b(options.context().getObject().find("noun").getString());
            return a + " " + b;
        });
        dom::Object data;
        data.set("adjective", "happy");
        HandlebarsOptions options;
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "happy cat");
    }

    // data can be looked up via @foo
    {
        std::string string = "{{@hello}}";
        dom::Object ctx;
        ctx.set("noun", "cat");
        dom::Object data;
        data.set("hello", "hello");
        HandlebarsOptions options;
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "hello");
    }

    // deep @foo triggers automatic top-level data
    {
        std::string string = "{{#let world=\"world\"}}{{#if foo}}{{#if foo}}Hello {{@world}}{{/if}}{{/if}}{{/let}}";
        dom::Object ctx;
        ctx.set("foo", true);
        hbs.registerHelper("let", [](dom::Array const& args, HandlebarsCallback const& options) {
            dom::Object data = createFrame(options.data());
            for (auto [k, v]: options.hash())
            {
                data.set(k, v);
            }
            return options.fn(options.context(), data, {}, {});
        });
        BOOST_TEST(hbs.render(string, ctx) == "Hello world");
    }

    // parameter data can be looked up via @foo
    {
        std::string string = "{{hello @world}}";
        dom::Object data;
        data.set("world", "world");
        HandlebarsOptions options;
        options.data = data;
        hbs.registerHelper("hello", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string noun(args[0].getString());
            return "Hello " + noun;
        });
        BOOST_TEST(hbs.render(string, {}, options) == "Hello world");
    }

    // hash values can be looked up via @foo
    {
        std::string string = "{{hello noun=@world}}";
        dom::Object data;
        data.set("world", "world");
        HandlebarsOptions options;
        options.data = data;
        hbs.registerHelper("hello", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string noun(options.hash().find("noun").getString());
            return "Hello " + noun;
        });
        BOOST_TEST(hbs.render(string, {}, options) == "Hello world");
    }

    // nested parameter data can be looked up via @foo.bar
    {
        std::string string = "{{hello @world.bar}}";
        dom::Object data;
        dom::Object world;
        world.set("bar", "world");
        data.set("world", world);
        HandlebarsOptions options;
        options.data = data;
        hbs.registerHelper("hello", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string noun(args[0].getString());
            return "Hello " + noun;
        });
        BOOST_TEST(hbs.render(string, {}, options) == "Hello world");
    }

    // nested parameter data does not fail with @world.bar
    {
        std::string string = "{{hello @world.bar}}";
        dom::Object data;
        dom::Object world;
        world.set("bar", "world");
        data.set("foo", world);
        HandlebarsOptions options;
        options.data = data;
        hbs.registerHelper("hello", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string noun = toString(args[0]);
            return "Hello " + noun;
        });
        // There's no undefined in this dom implementation
        // BOOST_TEST(hbs.render(string, {}, options) == "Hello undefined");
        BOOST_TEST(hbs.render(string, {}, options) == "Hello null");
    }

    // parameter data throws when using complex scope references
    {
        dom::Object ctx;
        ctx.set("goodbyes", true);
        BOOST_TEST_THROW_WITH(
            hbs.render("{{#goodbyes}}{{text}} cruel {{@foo/../name}}! {{/goodbyes}}", ctx),
            HandlebarsError, "Invalid path: @foo/.. - 1:30");
    }

    // data can be functions
    {
        std::string string = "{{@hello}}";
        dom::Object ctx;
        dom::Object data;
        data.set("hello", dom::makeInvocable([]() {
            return dom::Value("hello");
        }));
        HandlebarsOptions options;
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "hello");
    }

    // data can be functions with params
    {
        std::string string = "{{@hello \"hello\"}}";
        dom::Object ctx;
        dom::Object data;
        data.set("hello", dom::makeInvocable([](dom::Value const& arg) {
            return arg;
        }));
        HandlebarsOptions options;
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "hello");
    }

    // data is inherited downstream
    {
        std::string string = "{{#let foo=1 bar=2}}{{#let foo=bar.baz}}{{@bar}}{{@foo}}{{/let}}{{@foo}}{{/let}}";
        dom::Object ctx;
        dom::Object bar;
        bar.set("baz", "hello world");
        ctx.set("bar", bar);
        hbs.registerHelper("let", [](dom::Array const& args, HandlebarsCallback const& options) {
            dom::Object frame = createFrame(options.data());
            for (auto [k, v]: options.hash())
            {
                frame.set(k, v);
            }
            return options.fn(options.context(), frame, {}, {});
        });
        HandlebarsOptions options;
        options.data = dom::Object();
        BOOST_TEST(hbs.render(string, ctx, options) == "2hello world1");
    }

    // passing in data to a compiled function that expects data - works with helpers in partials
    {
        std::string string = "{{>myPartial}}";
        hbs.registerPartial("myPartial", "{{hello}}");
        hbs.registerHelper("hello", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string a(options.data().find("adjective").getString());
            std::string b(options.context().getObject().find("noun").getString());
            return a + " " + b;
        });
        dom::Object ctx;
        ctx.set("noun", "cat");
        HandlebarsOptions options;
        dom::Object data;
        data.set("adjective", "happy");
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "happy cat");
    }

    // passing in data to a compiled function that expects data - works with helpers and parameters
    {
        std::string string = "{{hello world}}";
        hbs.registerHelper("hello", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string noun(args[0].getString());
            std::string adjective(options.data().find("adjective").getString());
            std::string exclaim(options.context().getObject().find("exclaim").getString());
            return adjective + " " + noun + (exclaim == "true" ? "!" : "");
        });
        dom::Object ctx;
        ctx.set("world", "world");
        ctx.set("exclaim", "true");
        HandlebarsOptions options;
        dom::Object data;
        data.set("adjective", "happy");
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "happy world!");
    }

    // passing in data to a compiled function that expects data - works with block helpers
    {
        std::string string = "{{#hello}}{{world}}{{/hello}}";
        hbs.registerHelper("hello", [](dom::Array const& args, HandlebarsCallback const& options) {
            return options.fn(options.context());
        });
        hbs.registerHelper("world", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string adjective(options.data().find("adjective").getString());
            bool exclaim = options.context().getObject().find("exclaim").getBool();
            return adjective + " world" + (exclaim ? "!" : "");
        });
        dom::Object ctx;
        ctx.set("exclaim", true);
        HandlebarsOptions options;
        dom::Object data;
        data.set("adjective", "happy");
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "happy world!");
    }

    // passing in data to a compiled function that expects data - works with block helpers that use ..
    {
        std::string string = "{{#hello}}{{world ../zomg}}{{/hello}}";
        hbs.registerHelper("hello", [](dom::Array const& args, HandlebarsCallback const& options) {
            dom::Object newContext;
            newContext.set("exclaim", "?");
            return options.fn(newContext);
        });
        hbs.registerHelper("world", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string thing(args[0].getString());
            std::string adjective(options.data().find("adjective").getString());
            dom::Value exclaim = options.context().getObject().find("exclaim");
            if (isTruthy(exclaim))
            {
                std::string exclaimString = toString(exclaim);
                return adjective + " " + thing + exclaimString;
            }
            else
            {
                return adjective + " " + thing;
            }
        });
        dom::Object ctx;
        ctx.set("exclaim", true);
        ctx.set("zomg", "world");
        HandlebarsOptions options;
        dom::Object data;
        data.set("adjective", "happy");
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "happy world?");
    }

    // passing in data to a compiled function that expects data - data is passed to with block helpers where children use ..
    {
        std::string string = "{{#hello}}{{world ../zomg}}{{/hello}}";
        hbs.registerHelper("hello", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string res;
            res += options.data().find("accessData").getString();
            res += " ";
            dom::Object newContext;
            newContext.set("exclaim", "?");
            res += options.fn(newContext);
            return res;
        });
        hbs.registerHelper("world", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string thing(args[0].getString());
            std::string adjective(options.data().find("adjective").getString());
            dom::Value exclaim = options.context().getObject().find("exclaim");
            if (isTruthy(exclaim))
            {
                std::string exclaimString = toString(exclaim);
                return adjective + " " + thing + exclaimString;
            }
            else
            {
                return adjective + " " + thing;
            }
        });
        dom::Object ctx;
        ctx.set("exclaim", true);
        ctx.set("zomg", "world");
        HandlebarsOptions options;
        dom::Object data;
        data.set("adjective", "happy");
        data.set("accessData", "#win");
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "#win happy world?");
    }

    // you can override inherited data when invoking a helper
    {
        std::string string = "{{#hello}}{{world zomg}}{{/hello}}";
        hbs.registerHelper("hello", [](dom::Array const&, HandlebarsCallback const& options) {
            dom::Object newContext;
            newContext.set("exclaim", "?");
            newContext.set("zomg", "world");
            dom::Object newData;
            newData.set("adjective", "sad");
            return options.fn(newContext, newData, {}, {});
        });
        hbs.registerHelper("world", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string thing(args[0].getString());
            std::string adjective(options.data().find("adjective").getString());
            dom::Value exclaim = options.context().getObject().find("exclaim");
            if (isTruthy(exclaim))
            {
                std::string exclaimString = toString(exclaim);
                return adjective + " " + thing + exclaimString;
            }
            else
            {
                return adjective + " " + thing;
            }
        });
        dom::Object ctx;
        ctx.set("exclaim", true);
        ctx.set("zomg", "planet");
        HandlebarsOptions options;
        dom::Object data;
        data.set("adjective", "happy");
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "sad world?");
    }

    // you can override inherited data when invoking a helper with depth
    {
        std::string string = "{{#hello}}{{world ../zomg}}{{/hello}}";
        hbs.registerHelper("hello", [](dom::Array const&, HandlebarsCallback const& options) {
            dom::Object newContext;
            newContext.set("exclaim", "?");
            dom::Object newData;
            newData.set("adjective", "sad");
            return options.fn(newContext, newData, {}, {});
        });
        hbs.registerHelper("world", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string thing(args[0].getString());
            std::string adjective(options.data().find("adjective").getString());
            dom::Value exclaim = options.context().getObject().find("exclaim");
            if (isTruthy(exclaim))
            {
                std::string exclaimString = toString(exclaim);
                return adjective + " " + thing + exclaimString;
            }
            else
            {
                return adjective + " " + thing;
            }
        });
        dom::Object ctx;
        ctx.set("exclaim", true);
        ctx.set("zomg", "world");
        HandlebarsOptions options;
        dom::Object data;
        data.set("adjective", "happy");
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "sad world?");
    }

    // @root
    {
        // the root context can be looked up via @root
        std::string string = "{{@root.foo}}";
        dom::Object ctx;
        ctx.set("foo", "hello");
        HandlebarsOptions options;
        options.data = dom::Object();
        BOOST_TEST(hbs.render(string, ctx, options) == "hello");
        BOOST_TEST(hbs.render(string, ctx) == "hello");

        // passed root values take priority
        ctx.set("foo", "should not be used");
        dom::Object data;
        dom::Object root;
        root.set("foo", "hello");
        data.set("root", root);
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "hello");
    }

    // nesting
    {
        std::string string = "{{#helper}}{{#helper}}{{@./depth}} {{@../depth}} {{@../../depth}}{{/helper}}{{/helper}}";
        hbs.registerHelper("helper", [](dom::Array const& args, HandlebarsCallback const& options) {
            dom::Object frame = createFrame(options.data());
            frame.set("depth", options.data().find("depth").getInteger() + 1);
            return options.fn(options.context(), frame, {}, {});
        });
        dom::Object ctx;
        ctx.set("foo", "hello");
        HandlebarsOptions options;
        dom::Object data;
        data.set("depth", 0);
        options.data = data;
        BOOST_TEST(hbs.render(string, ctx, options) == "2 1 0");
    }
}

void
helpers()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/helpers.js
    Handlebars hbs;

    // helper with complex lookup
    {
        std::string string = "{{#goodbyes}}{{{link ../prefix}}}{{/goodbyes}}";
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "Goodbye");
        goodbye1.set("url", "goodbye");
        goodbyes.emplace_back(goodbye1);
        ctx.set("goodbyes", goodbyes);
        ctx.set("prefix", "/root");
        hbs.registerHelper("link", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string prefix(args[0].getString());
            std::string url(options.context().getObject().find("url").getString());
            std::string text(options.context().getObject().find("text").getString());
            return "<a href=\"" + prefix + "/" + url + "\">" + text + "</a>";
        });
        BOOST_TEST(hbs.render(string, ctx) == "<a href=\"/root/goodbye\">Goodbye</a>");
    }

    // helper for raw block gets raw content
    {
        std::string string = "{{{{raw}}}} {{test}} {{{{/raw}}}}";
        dom::Object ctx;
        ctx.set("test", "hello");
        hbs.registerHelper("raw", [](dom::Array const&, HandlebarsCallback const& options) {
            return options.fn();
        });
        BOOST_TEST(hbs.render(string, ctx) == " {{test}} ");
    }

    // helper for raw block gets parameters
    {
        std::string string = "{{{{raw 1 2 3}}}} {{test}} {{{{/raw}}}}";
        dom::Object ctx;
        ctx.set("test", "hello");
        hbs.registerHelper("raw", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string res = options.fn();
            for (auto const& arg: args)
            {
                res += toString(arg);
            }
            return res;
        });
        BOOST_TEST(hbs.render(string, ctx) == " {{test}} 123");
    }

    // raw block parsing (with identity helper-function)
    {
        hbs.registerHelper("identity", [](dom::Array const&, HandlebarsCallback const& options) {
            return options.fn();
        });
        // helper for nested raw block gets raw content
        std::string string = "{{{{identity}}}} {{{{b}}}} {{{{/b}}}} {{{{/identity}}}}";
        BOOST_TEST(hbs.render(string) == " {{{{b}}}} {{{{/b}}}} ");

        // helper for nested raw block works with empty content
        string = "{{{{identity}}}}{{{{/identity}}}}";
        BOOST_TEST(hbs.render(string) == "");

        // helper for nested raw block works if nested raw blocks are broken
        string = "{{{{identity}}}} {{{{a}}}} {{{{ {{{{/ }}}} }}}} {{{{/identity}}}}";
        BOOST_TEST(hbs.render(string) == " {{{{a}}}} {{{{ {{{{/ }}}} }}}} ");

        // helper for nested raw block closes after first matching close
        string = "{{{{identity}}}}abc{{{{/identity}}}} {{{{identity}}}}abc{{{{/identity}}}}";
        BOOST_TEST(hbs.render(string) == "abc abc");

        // helper for nested raw block throw exception when with missing closing braces
        string = "{{{{a}}}} {{{{/a";
        BOOST_TEST_THROW_WITH(
            hbs.render(string),
            HandlebarsError, "a missing closing braces - 1:4");
    }

    // helper block with identical context
    {
        std::string string = "{{#goodbyes}}{{name}}{{/goodbyes}}";
        dom::Object ctx;
        ctx.set("name", "Alan");
        hbs.registerHelper("goodbyes", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string out;
            out += "Goodbye " + options.fn(options.context()) + "! ";
            out += "goodbye " + options.fn(options.context()) + "! ";
            out += "GOODBYE " + options.fn(options.context()) + "! ";
            return out;
        });
        BOOST_TEST(hbs.render(string, ctx) == "Goodbye Alan! goodbye Alan! GOODBYE Alan! ");
    }

    // helper block with complex lookup expression
    {
        std::string string = "{{#goodbyes}}{{../name}}{{/goodbyes}}";
        dom::Object ctx;
        ctx.set("name", "Alan");
        hbs.registerHelper("goodbyes", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string out;
            dom::Object newContext = {};
            out += "Goodbye " + options.fn(newContext) + "! ";
            out += "goodbye " + options.fn(newContext) + "! ";
            out += "GOODBYE " + options.fn(newContext) + "! ";
            return out;
        });
        BOOST_TEST(hbs.render(string, ctx) == "Goodbye Alan! goodbye Alan! GOODBYE Alan! ");
        hbs.unregisterHelper("goodbyes");
    }

    // helper with complex lookup and nested template
    {
        std::string string = "{{#goodbyes}}{{#link ../prefix}}{{text}}{{/link}}{{/goodbyes}}";
        dom::Object ctx;
        dom::Array goodbyes;
        dom::Object goodbye1;
        goodbye1.set("text", "Goodbye");
        goodbye1.set("url", "goodbye");
        goodbyes.emplace_back(goodbye1);
        ctx.set("goodbyes", goodbyes);
        ctx.set("prefix", "/root");
        hbs.registerHelper("link", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string prefix(args[0].getString());
            std::string url(options.context().getObject().find("url").getString());
            std::string text(options.context().getObject().find("text").getString());
            return "<a href=\"" + prefix + "/" + url + "\">" + text + "</a>";
        });
        BOOST_TEST(hbs.render(string, ctx) == "<a href=\"/root/goodbye\">Goodbye</a>");
    }

    // helper returning undefined value
    {
        std::string string = " {{nothere}}";
        hbs.registerHelper("nothere", [](dom::Array const&, HandlebarsCallback const&) {
            return dom::Value();
        });
        BOOST_TEST(hbs.render(string) == " ");

        string = " {{#nothere}}{{/nothere}}";
        BOOST_TEST(hbs.render(string) == " ");
    }

    // block helper
    {
        std::string string = "{{#goodbyes}}{{text}}! {{/goodbyes}}cruel {{world}}!";
        dom::Object ctx;
        ctx.set("world", "world");
        hbs.registerHelper("goodbyes", [](dom::Array const& args, HandlebarsCallback const& options) {
            dom::Object ctx;
            ctx.set("text", "GOODBYE");
            return options.fn(ctx);
        });
        BOOST_TEST(hbs.render(string, ctx) == "GOODBYE! cruel world!");
    }

    // block helper staying in the same context
    {
        std::string string = "{{#form}}<p>{{name}}</p>{{/form}}";
        dom::Object ctx;
        ctx.set("name", "Yehuda");
        hbs.registerHelper("form", [](dom::Array const&, HandlebarsCallback const& options) {
            return "<form>" + options.fn() + "</form>";
        });
        BOOST_TEST(hbs.render(string, ctx) == "<form><p>Yehuda</p></form>");
    }

    // block helper should have context in this
    {
        std::string string = "<ul>{{#people}}<li>{{#link}}{{name}}{{/link}}</li>{{/people}}</ul>";
        dom::Object ctx;
        dom::Array people;
        dom::Object person1;
        person1.set("name", "Alan");
        person1.set("id", 1);
        people.emplace_back(person1);
        dom::Object person2;
        person2.set("name", "Yehuda");
        person2.set("id", 2);
        people.emplace_back(person2);
        ctx.set("people", people);
        hbs.registerHelper("link", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string out;
            out += "<a href=\"/people/" + toString(options.context().getObject().find("id")) + "\">";
            out += options.fn();
            out += "</a>";
            return out;
        });
        BOOST_TEST(hbs.render(string, ctx) == "<ul><li><a href=\"/people/1\">Alan</a></li><li><a href=\"/people/2\">Yehuda</a></li></ul>");
    }

    // block helper for undefined value
    {
        std::string string = "{{#empty}}shouldn't render{{/empty}}";
        BOOST_TEST(hbs.render(string) == "");
    }

    // block helper passing a new context
    {
        std::string string = "{{#form yehuda}}<p>{{name}}</p>{{/form}}";
        dom::Object ctx;
        dom::Object yehuda;
        yehuda.set("name", "Yehuda");
        ctx.set("yehuda", yehuda);
        hbs.registerHelper("form", [](dom::Array const& args, HandlebarsCallback const& options) {
            return "<form>" + options.fn(args[0]) + "</form>";
        });
        BOOST_TEST(hbs.render(string, ctx) == "<form><p>Yehuda</p></form>");
    }

    // block helper passing a complex path context
    {
        std::string string = "{{#form yehuda/cat}}<p>{{name}}</p>{{/form}}";
        dom::Object ctx;
        dom::Object yehuda;
        dom::Object cat;
        cat.set("name", "Harold");
        yehuda.set("name", "Yehuda");
        yehuda.set("cat", cat);
        ctx.set("yehuda", yehuda);
        hbs.registerHelper("form", [](dom::Array const& args, HandlebarsCallback const& options) {
            return "<form>" + options.fn(args[0]) + "</form>";
        });
        BOOST_TEST(hbs.render(string, ctx) == "<form><p>Harold</p></form>");
    }

    // nested block helpers
    {
        std::string string = "{{#form yehuda}}<p>{{name}}</p>{{#link}}Hello{{/link}}{{/form}}";
        dom::Object ctx;
        dom::Object yehuda;
        yehuda.set("name", "Yehuda");
        ctx.set("yehuda", yehuda);
        hbs.registerHelper("link", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string out;
            out += "<a href=\"";
            out += options.context().getObject().find("name").getString();
            out += "\">";
            out += options.fn(options.context());
            out += "</a>";
            return out;
        });
        hbs.registerHelper("form", [](dom::Array const& args, HandlebarsCallback const& options) {
            return "<form>" + options.fn(args[0]) + "</form>";
        });
        BOOST_TEST(hbs.render(string, ctx) == "<form><p>Yehuda</p><a href=\"Yehuda\">Hello</a></form>");
    }

    // block helper inverted sections
    {
        std::string string = "{{#list people}}{{name}}{{^}}<em>Nobody's here</em>{{/list}}";
        hbs.registerHelper("list", [](dom::Array const& args, HandlebarsCallback const& options) {
            dom::Array people = args[0].getArray();
            if (!people.empty())
            {
                std::string out = "<ul>";
                for (auto const& person: people)
                {
                    out += "<li>";
                    out += options.fn(person);
                    out += "</li>";
                }
                out += "</ul>";
                return out;
            }
            else
            {
                return "<p>" + options.inverse(options.context()) + "</p>";
            }
        });

        // an inverse wrapper is passed in as a new context
        dom::Object ctx;
        dom::Array people;
        dom::Object person1;
        person1.set("name", "Alan");
        people.emplace_back(person1);
        dom::Object person2;
        person2.set("name", "Yehuda");
        people.emplace_back(person2);
        ctx.set("people", people);
        BOOST_TEST(hbs.render(string, ctx) == "<ul><li>Alan</li><li>Yehuda</li></ul>");

        // an inverse wrapper can be optionally called
        ctx.set("people", dom::Array());
        BOOST_TEST(hbs.render(string, ctx) == "<p><em>Nobody's here</em></p>");

        // the context of an inverse is the parent of the block
        string = "{{#list people}}Hello{{^}}{{message}}{{/list}}";
        ctx.set("message", "Nobody's here");
        BOOST_TEST(hbs.render(string, ctx) == "<p>Nobody&#x27;s here</p>");
    }

    // pathed lambas with parameters
    {
        dom::Object hash;
        dom::Function helper = dom::makeInvocable([](dom::Value const& arg) {
            return dom::Value("winning");
        });
        hash.set("helper", helper);
        // hash.set("hash", hash);
        dom::Object hash2;
        hash2.set("helper", helper);
        hash.set("hash", hash2);
        hbs.registerHelper("./helper", []() {
            return "fail";
        });
        BOOST_TEST(hbs.render("{{./helper 1}}", hash) == "fail");
        BOOST_TEST(hbs.render("{{hash/helper 1}}", hash) == "winning");
    }

    // helpers hash
    {
        // providing a helpers hash
        {
            std::string string = "Goodbye {{cruel}} {{world}}!";
            dom::Object ctx;
            ctx.set("cruel", "cruel");
            hbs.registerHelper("world", []() {
                return "world";
            });
            BOOST_TEST(hbs.render(string, ctx) == "Goodbye cruel world!");

            string = "Goodbye {{#iter}}{{cruel}} {{world}}{{/iter}}!";
            dom::Array iter;
            dom::Object iter1;
            iter1.set("cruel", "cruel");
            iter.emplace_back(iter1);
            ctx.set("iter", iter);
            hbs.registerHelper("world", []() {
                return "world";
            });
            BOOST_TEST(hbs.render(string, ctx) == "Goodbye cruel world!");
        }

        // in cases of conflict, helpers win
        {
            std::string string = "{{{lookup}}}";
            dom::Object ctx;
            ctx.set("lookup", "Explicit");
            hbs.registerHelper("lookup", []() {
                return "helpers";
            });
            BOOST_TEST(hbs.render(string, ctx) == "helpers");
            string = "{{lookup}}";
            BOOST_TEST(hbs.render(string, ctx) == "helpers");
        }

        // the helpers hash is available is nested contexts
        {
            std::string string = "{{#outer}}{{#inner}}{{helper}}{{/inner}}{{/outer}}";
            dom::Object ctx;
            dom::Object outer;
            dom::Object inner;
            dom::Array unused;
            inner.set("unused", unused);
            outer.set("inner", inner);
            ctx.set("outer", outer);
            hbs.registerHelper("helper", []() {
                return "helper";
            });
            BOOST_TEST(hbs.render(string, ctx) == "helper");
        }

        // the helper hash should augment the global hash
        {
            hbs.registerHelper("test_helper", []() {
                return "found it!";
            });
            std::string string = "{{test_helper}} {{#if cruel}}Goodbye {{cruel}} {{world}}!{{/if}}";
            dom::Object ctx;
            ctx.set("cruel", "cruel");
            hbs.registerHelper("world", []() {
                return "world!";
            });
            BOOST_TEST(hbs.render(string, ctx) == "found it! Goodbye cruel world!!");
        }
    }

    // registration
    {
        // unregisters
        {
            hbs = Handlebars();
            hbs.registerHelper("foo", []() {
                return "fail";
            });
            hbs.unregisterHelper("foo");
            BOOST_TEST(hbs.render("{{foo}}") == "");
        }

        // allows multiple globals
        {
            hbs = Handlebars();
            hbs.registerHelper("if", helpers::if_fn);
            hbs.registerHelper("world", []() {
                return "world!";
            });
            hbs.registerHelper("testHelper", []() {
                return "found it!";
            });
            std::string string = "{{testHelper}} {{#if cruel}}Goodbye {{cruel}} {{world}}!{{/if}}";
            dom::Object ctx;
            ctx.set("cruel", "cruel");
            BOOST_TEST(hbs.render(string, ctx) == "found it! Goodbye cruel world!!");
        }
    }

    // decimal number literals work
    {
        // The C++ dom implementation does not support floating point numbers
        // std::string string = "Message: {{hello -1.2 1.2}}";
        std::string string = "Message: {{hello -1 1}}";
        hbs.registerHelper("hello", [](dom::Array const& args) {
            return "Hello " + toString(args[0]) + " " + toString(args[1]) + " times";
        });
        // BOOST_TEST(hbs.render(string) == "Message: Hello -1.2 1.2 times");
        BOOST_TEST(hbs.render(string) == "Message: Hello -1 1 times");
    }

    // negative number literals work
    {
        std::string string = "Message: {{hello -12}}";
        hbs.registerHelper("hello", [](dom::Array const& args) {
            return "Hello " + toString(args[0]) + " times";
        });
        BOOST_TEST(hbs.render(string) == "Message: Hello -12 times");
    }

    // String literal parameters
    {
        // simple literals work
        {
            std::string string = "Message: {{hello \"world\" 12 true false}}";
            hbs.registerHelper("hello", [](dom::Array const& args) {
                std::string param(args[0].getString());
                std::int64_t times = args[1].getInteger();
                bool bool1 = args[2].getBool();
                bool bool2 = args[3].getBool();
                return "Hello " + param + " " + std::to_string(times) + " times: " + (bool1 ? "true" : "false") + " " + (bool2 ? "true" : "false");
            });
            BOOST_TEST(hbs.render(string) == "Message: Hello world 12 times: true false");
        }

        // using a quote in the middle of a parameter raises an error
        {
            BOOST_TEST_THROWS(
                hbs.render("Message: {{hello wo\"rld\"}}"), HandlebarsError);
        }

        // escaping a String is possible
        {
            std::string string = "Message: {{{hello \"\\\"world\\\"\"}}}";
            hbs.registerHelper("hello", [](dom::Array const& args) {
                std::string param(args[0].getString());
                return "Hello " + param;
            });
            BOOST_TEST(hbs.render(string) == "Message: Hello \"world\"");
        }

        // it works with ' marks
        {
            std::string string = "Message: {{{hello \"Alan's world\"}}}";
            hbs.registerHelper("hello", [](dom::Array const& args) {
                std::string param(args[0].getString());
                return "Hello " + param;
            });
            BOOST_TEST(hbs.render(string) == "Message: Hello Alan's world");
        }

        // negative number literals work
        {
            std::string string = "Message: {{hello -12}}";
            hbs.registerHelper("hello", [](dom::Array const& args) {
                return "Hello " + toString(args[0]) + " times";
            });
            BOOST_TEST(hbs.render(string) == "Message: Hello -12 times");
        }
    }

    // multiple parameters
    {
        // simple multi-params work
        {
            std::string string = "Message: {{goodbye cruel world}}";
            hbs.registerHelper("goodbye", [](dom::Array const& args) {
                std::string cruel(args[0].getString());
                std::string world(args[1].getString());
                return "Goodbye " + cruel + " " + world;
            });
            dom::Object ctx;
            ctx.set("cruel", "cruel");
            ctx.set("world", "world");
            BOOST_TEST(hbs.render(string, ctx) == "Message: Goodbye cruel world");
        }

        // block multi-params work
        {
            std::string string = "Message: {{#goodbye cruel world}}{{greeting}} {{adj}} {{noun}}{{/goodbye}}";
            hbs.registerHelper("goodbye", [](dom::Array const& args, HandlebarsCallback const& options) {
                dom::Object ctx;
                ctx.set("greeting", "Goodbye");
                ctx.set("adj", args[0].getString());
                ctx.set("noun", args[1].getString());
                return options.fn(ctx);
            });
            dom::Object ctx;
            ctx.set("cruel", "cruel");
            ctx.set("world", "world");
            BOOST_TEST(hbs.render(string, ctx) == "Message: Goodbye cruel world");
        }
    }

    // hash
    {
        // helpers can take an optional hash
        {
            std::string string = "{{goodbye cruel=\"CRUEL\" world=\"WORLD\" times=12}}";
            hbs.registerHelper("goodbye", [](dom::Array const& args, HandlebarsCallback const& options) {
                std::string out;
                out += "GOODBYE ";
                out += options.hash().find("cruel").getString();
                out += " ";
                out += options.hash().find("world").getString();
                out += " ";
                out += toString(options.hash().find("times"));
                out += " TIMES";
                return out;
            });
            BOOST_TEST(hbs.render(string) == "GOODBYE CRUEL WORLD 12 TIMES");
        }

        // helpers can take an optional hash with booleans
        {
            hbs.registerHelper("goodbye", [](dom::Array const& args, HandlebarsCallback const& options) -> std::string {
                if (options.hash().find("print").getBool())
                {
                    std::string out;
                    out += "GOODBYE ";
                    out += options.hash().find("cruel").getString();
                    out += " ";
                    out += options.hash().find("world").getString();
                    return out;
                }
                else if (!options.hash().find("print").getBool())
                {
                    return "NOT PRINTING";
                }
                else
                {
                    return "THIS SHOULD NOT HAPPEN";
                }
            });
            std::string string = "{{goodbye cruel=\"CRUEL\" world=\"WORLD\" print=true}}";
            BOOST_TEST(hbs.render(string) == "GOODBYE CRUEL WORLD");
            string = "{{goodbye cruel=\"CRUEL\" world=\"WORLD\" print=false}}";
            BOOST_TEST(hbs.render(string) == "NOT PRINTING");
        }

        // block helpers can take an optional hash
        {
            hbs.registerHelper("goodbye", [](dom::Array const& args, HandlebarsCallback const& options) {
                std::string out;
                out += "GOODBYE ";
                out += options.hash().find("cruel").getString();
                out += " ";
                out += options.fn(options.context());
                out += " ";
                out += toString(options.hash().find("times"));
                out += " TIMES";
                return out;
            });
            std::string string = "{{#goodbye cruel=\"CRUEL\" times=12}}world{{/goodbye}}";
            BOOST_TEST(hbs.render(string) == "GOODBYE CRUEL world 12 TIMES");
        }

        // block helpers can take an optional hash with single quoted stings
        {
            std::string string = "{{#goodbye cruel=\"CRUEL\" times=12}}world{{/goodbye}}";
            hbs.registerHelper("goodbye", [](dom::Array const& args, HandlebarsCallback const& options) {
                std::string out;
                out += "GOODBYE ";
                out += options.hash().find("cruel").getString();
                out += " ";
                out += options.fn(options.context());
                out += " ";
                out += toString(options.hash().find("times"));
                out += " TIMES";
                return out;
            });
            BOOST_TEST(hbs.render(string) == "GOODBYE CRUEL world 12 TIMES");
        }

        // block helpers can take an optional hash with booleans
        {
            hbs.registerHelper("goodbye", [](dom::Array const& args, HandlebarsCallback const& options) -> std::string {
                if (options.hash().find("print").getBool())
                {
                    std::string out;
                    out += "GOODBYE ";
                    out += options.hash().find("cruel").getString();
                    out += " ";
                    out += options.fn(options.context());
                    return out;
                }
                else if (!options.hash().find("print").getBool())
                {
                    return "NOT PRINTING";
                }
                else
                {
                    return "THIS SHOULD NOT HAPPEN";
                }
            });
            std::string string = "{{#goodbye cruel=\"CRUEL\" print=true}}world{{/goodbye}}";
            BOOST_TEST(hbs.render(string) == "GOODBYE CRUEL world");
            string = "{{#goodbye cruel=\"CRUEL\" print=false}}world{{/goodbye}}";
            BOOST_TEST(hbs.render(string) == "NOT PRINTING");
        }
    }

    hbs = {};

    // helperMissing
    {
        // if a context is not found, helperMissing is used
        {
            std::string string = "{{hello}} {{link_to world}}";
            BOOST_TEST_THROW_WITH(
                hbs.render(string), std::runtime_error, "Missing helper: \"link_to\"");
        }

        // if a context is not found, custom helperMissing is used
        {
            std::string string = "{{hello}} {{link_to world}}";
            hbs.registerHelper("helperMissing", [](dom::Array const& args, HandlebarsCallback const& options) {
                if (options.name() == "link_to")
                {
                    return safeString("<a>" + std::string(args[0].getString()) + "</a>");
                }
                return safeString("");
            });
            dom::Object ctx;
            ctx.set("hello", "Hello");
            ctx.set("world", "world");
            BOOST_TEST(hbs.render(string, ctx) == "Hello <a>world</a>");
        }

        // if a value is not found, custom helperMissing is used
        {
            std::string string = "{{hello}} {{link_to}}";
            hbs.registerHelper("helperMissing", [](dom::Array const& args, HandlebarsCallback const& options) {
                if (options.name() == "link_to")
                {
                    return safeString("<a>winning</a>");
                }
                return safeString("");
            });
            dom::Object ctx;
            ctx.set("hello", "Hello");
            ctx.set("world", "world");
            BOOST_TEST(hbs.render(string, ctx) == "Hello <a>winning</a>");
        }
    }

    // blockHelperMissing
    // These tests deviate from the original Handlebars.js tests since
    // context lambdas cannot receive HandlebarsCallback.
    {
        // lambdas are resolved by blockHelperMissing, not handlebars proper
        {
            std::string string = "{{#truthy}}yep{{/truthy}}";
            dom::Object ctx;
            ctx.set("truthy", dom::makeInvocable([]() {
                return true;
            }));
            BOOST_TEST(hbs.render(string, ctx) == "yep");
        }

        // lambdas resolved by blockHelperMissing are bound to the context
        {
            std::string string = "{{#truthy}}yep{{/truthy}}";
            hbs.registerHelper("truthy", [](dom::Array const& args, HandlebarsCallback const& options) {
                bool const v =
                    options.context().getObject().find("truthiness").getFunction()().getBool();
                if (v)
                {
                    return options.fn(options.context());
                }
                else
                {
                    return options.inverse(options.context());
                }
            });
            dom::Object ctx;
            ctx.set("truthiness", dom::makeInvocable([]() {
                return false;
            }));
            BOOST_TEST(hbs.render(string, ctx) == "");
        }
    }

    // name field
    {
        hbs = {};
        hbs.registerHelper("blockHelperMissing", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string res = "missing: ";
            res += options.name();
            return res;
        });
        hbs.registerHelper("helperMissing", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string res = "helper missing: ";
            res += options.name();
            return res;
        });
        hbs.registerHelper("helper", [](dom::Array const& args, HandlebarsCallback const& options) {
            std::string res = "ran: ";
            res += options.name();
            return res;
        });

        // should include in ambiguous mustache calls
        {
            BOOST_TEST(hbs.render("{{helper}}") == "ran: helper");
        }

        // should include in helper mustache calls
        {
            BOOST_TEST(hbs.render("{{helper 1}}") == "ran: helper");
        }

        // should include in ambiguous block calls
        {
            BOOST_TEST(hbs.render("{{#helper}}{{/helper}}") == "ran: helper");
        }

        // should include in simple block calls
        {
            BOOST_TEST(hbs.render("{{#./helper}}{{/./helper}}") == "missing: ./helper");
        }

        // should include in helper block calls
        {
            // expectTemplate('{{#helper 1}}{{/helper}}')
            //        .withHelpers(helpers)
            //        .toCompileTo('ran: helper');
            BOOST_TEST(hbs.render("{{#helper 1}}{{/helper}}") == "ran: helper");
        }

        // should include in known helper calls
        {
            BOOST_TEST(hbs.render("{{helper}}") == "ran: helper");
        }

        // should include full id
        {
            // expectTemplate('{{#foo.helper}}{{/foo.helper}}')
            //        .withInput({ foo: {} })
            //        .withHelpers(helpers)
            //        .toCompileTo('missing: foo.helper');
            dom::Object ctx;
            ctx.set("foo", dom::Object());
            BOOST_TEST(hbs.render("{{#foo.helper}}{{/foo.helper}}", ctx) == "missing: foo.helper");
        }

        // should include full id if a hash is passed
        {
            dom::Object ctx;
            ctx.set("foo", dom::Object());
            BOOST_TEST(hbs.render("{{#foo.helper bar=baz}}{{/foo.helper}}", ctx) == "helper missing: foo.helper");
        }
    }

    // name conflicts
    {
        // helpers take precedence over same-named context properties
        {
            dom::Object ctx;
            ctx.set("goodbye", "goodbye");
            ctx.set("world", "world");
            hbs.registerHelper("goodbye", [](dom::Array const& args, HandlebarsCallback const& options) {
                std::string res = toString(options.context().getObject().find("goodbye"));
                std::transform(res.begin(), res.end(), res.begin(), [](char c) {
                    return static_cast<char>(std::toupper(c));
                });
                return res;
            });
            hbs.registerHelper("cruel", [](dom::Array const& args, HandlebarsCallback const& options) {
                std::string world = toString(args[0]);
                std::transform(world.begin(), world.end(), world.begin(), [](char c) {
                    return static_cast<char>(std::toupper(c));
                });
                return "cruel " + world;
            });
            BOOST_TEST(hbs.render("{{goodbye}} {{cruel world}}", ctx) == "GOODBYE cruel WORLD");
        }

        // helpers take precedence over same-named context properties
        {
            dom::Object ctx;
            ctx.set("goodbye", "goodbye");
            ctx.set("world", "world");
            hbs.registerHelper("goodbye", [](dom::Array const& args, HandlebarsCallback const& options) {
                std::string res = toString(options.context().getObject().find("goodbye"));
                std::transform(res.begin(), res.end(), res.begin(), [](char c) {
                    return static_cast<char>(std::toupper(c));
                });
                return res + options.fn(options.context());
            });
            hbs.registerHelper("cruel", [](dom::Array const& args, HandlebarsCallback const& options) {
                std::string world = toString(args[0]);
                std::transform(world.begin(), world.end(), world.begin(), [](char c) {
                    return static_cast<char>(std::toupper(c));
                });
                return "cruel " + world;
            });
            BOOST_TEST(hbs.render("{{#goodbye}} {{cruel world}}{{/goodbye}}", ctx) == "GOODBYE cruel WORLD");
        }

        // Scoped names take precedence over helpers
        {
            dom::Object ctx;
            ctx.set("goodbye", "goodbye");
            ctx.set("world", "world");
            hbs.registerHelper("goodbye", [](dom::Array const& args, HandlebarsCallback const& options) {
                std::string res = toString(options.context().getObject().find("goodbye"));
                std::transform(res.begin(), res.end(), res.begin(), [](char c) {
                return static_cast<char>(std::toupper(c));
                });
                return res;
            });
            hbs.registerHelper("cruel", [](dom::Array const& args, HandlebarsCallback const& options) {
                std::string world = toString(args[0]);
                std::transform(world.begin(), world.end(), world.begin(), [](char c) {
                return static_cast<char>(std::toupper(c));
                });
                return "cruel " + world;
            });
            BOOST_TEST(hbs.render("{{this.goodbye}} {{cruel world}} {{cruel this.goodbye}}", ctx) == "goodbye cruel WORLD cruel GOODBYE");
        }

        // Scoped names take precedence over block helpers
        {
            dom::Object ctx;
            ctx.set("goodbye", "goodbye");
            ctx.set("world", "world");
            hbs.registerHelper("goodbye", [](dom::Array const& args, HandlebarsCallback const& options) {
                std::string res = toString(options.context().getObject().find("goodbye"));
                std::transform(res.begin(), res.end(), res.begin(), [](char c) {
                    return static_cast<char>(std::toupper(c));
                });
                return res + options.fn(options.context());
            });
            hbs.registerHelper("cruel", [](dom::Array const& args, HandlebarsCallback const& options) {
                std::string world = toString(args[0]);
                std::transform(world.begin(), world.end(), world.begin(), [](char c) {
                    return static_cast<char>(std::toupper(c));
                });
                return "cruel " + world;
            });
            BOOST_TEST(hbs.render("{{#goodbye}} {{cruel world}}{{/goodbye}} {{this.goodbye}}", ctx) == "GOODBYE cruel WORLD goodbye");
        }
    }

    // block params
    {
        // should take precedence over context values
        {
            dom::Object ctx;
            ctx.set("value", "foo");
            hbs.registerHelper("goodbyes", [](dom::Array const& args, HandlebarsCallback const& options) {
                BOOST_TEST(options.blockParams() == 1u);
                dom::Object ctx;
                ctx.set("value", "bar");
                dom::Array blockParams;
                blockParams.emplace_back(1);
                return options.fn(ctx, options.data(), blockParams, {});
            });
            BOOST_TEST(hbs.render("{{#goodbyes as |value|}}{{value}}{{/goodbyes}}{{value}}", ctx) == "1foo");
        }

        // should take precedence over helper values
        {
            std::string string = "{{#goodbyes as |value|}}{{value}}{{/goodbyes}}{{value}}";
            hbs.registerHelper("value", [](dom::Array const& args, HandlebarsCallback const& options) {
                return "foo";
            });
            hbs.registerHelper("goodbyes", [](dom::Array const& args, HandlebarsCallback const& options) {
                BOOST_TEST(options.blockParams() == 1u);
                dom::Array blockParams;
                blockParams.emplace_back(1);
                return options.fn({}, options.data(), blockParams, {});
            });
            BOOST_TEST(hbs.render(string) == "1foo");
        }

        // should not take precedence over pathed values
        {
            dom::Object ctx;
            ctx.set("value", "bar");
            hbs.registerHelper("value", []() {
                return "foo";
            });
            hbs.registerHelper("goodbyes", [](dom::Array const& args, HandlebarsCallback const& options) {
                BOOST_TEST(options.blockParams() == 1u);
                dom::Array blockParams;
                blockParams.emplace_back(1);
                return options.fn(options.context(), options.data(), blockParams, {});
            });
            BOOST_TEST(hbs.render("{{#goodbyes as |value|}}{{./value}}{{/goodbyes}}{{value}}", ctx) == "barfoo");
        }

        // should take precedence over parent block params
        {
            dom::Object ctx;
            ctx.set("value", "foo");
            int value = 1;
            hbs.registerHelper("goodbyes", [&value](dom::Array const& args, HandlebarsCallback const& options) {
                dom::Object ctx;
                ctx.set("value", "bar");
                dom::Array blockParams;
                if (options.blockParams() != 0)
                {
                    blockParams.emplace_back(value);
                    value += 2;
                }
                return options.fn(ctx, options.data(), blockParams, {});
            });
            std::string string = "{{#goodbyes as |value|}}{{#goodbyes}}{{value}}{{#goodbyes as |value|}}{{value}}{{/goodbyes}}{{/goodbyes}}{{/goodbyes}}{{value}}";
            BOOST_TEST(hbs.render(string, ctx) == "13foo");
        }

        // should allow block params on chained helpers
        {
            dom::Object ctx;
            ctx.set("value", "foo");
            hbs.registerHelper("goodbyes", [](dom::Array const& args, HandlebarsCallback const& options) {
                BOOST_TEST(options.blockParams() == 1u);
                dom::Array blockParams;
                blockParams.emplace_back(1);
                dom::Object ctx;
                ctx.set("value", "bar");
                return options.fn(ctx, options.data(), blockParams, {});
            });
            BOOST_TEST(hbs.render("{{#if bar}}{{else goodbyes as |value|}}{{value}}{{/if}}{{value}}", ctx) == "1foo");
        }
    }

    // built-in helpers malformed arguments
    {
        // if helper - too few arguments
        {
            BOOST_TEST_THROW_WITH(
                hbs.render("{{#if}}{{/if}}"), HandlebarsError, "#if requires exactly one argument");
        }

        // if helper - too many arguments, string
        {
            BOOST_TEST_THROW_WITH(
                hbs.render("{{#if test \"string\"}}{{/if}}"), HandlebarsError, "#if requires exactly one argument");
        }

        // if helper - too many arguments, undefined
        {
            BOOST_TEST_THROW_WITH(
                hbs.render("{{#if test undefined}}{{/if}}"), HandlebarsError, "#if requires exactly one argument");
        }

        // if helper - too many arguments, null
        {
            BOOST_TEST_THROW_WITH(
                hbs.render("{{#if test null}}{{/if}}"), HandlebarsError, "#if requires exactly one argument");
        }

        // unless helper - too few arguments
        {
            BOOST_TEST_THROW_WITH(
                hbs.render("{{#unless}}{{/unless}}"), HandlebarsError, "#unless requires exactly one argument");
        }

        // unless helper - too many arguments, null
        {
            BOOST_TEST_THROW_WITH(
                hbs.render("{{#unless test null}}{{/unless}}"), HandlebarsError, "#unless requires exactly one argument");
        }

        // with helper - too few arguments
        {
            BOOST_TEST_THROW_WITH(
                hbs.render("{{#with}}{{/with}}"), HandlebarsError, "#with requires exactly one argument");
        }

        // with helper - too many arguments
        {
            BOOST_TEST_THROW_WITH(
                hbs.render("{{#with test \"string\"}}{{/with}}"), HandlebarsError, "#with requires exactly one argument");
        }
    }

    // the lookupProperty-option
    {
        // should be passed to custom helpers
        {
            hbs.registerHelper("testHelper", [](dom::Array const& args, HandlebarsCallback const& options) {
                return options.lookupProperty(options.context(), "testProperty").first;
            });
            dom::Object ctx;
            ctx.set("testProperty", "abc");
            BOOST_TEST(hbs.render("{{testHelper}}", ctx) == "abc");
        }
    }
}

void
track_ids()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/track-ids.js
    Handlebars hbs;

    // context = { is: { a: 'foo' }, slave: { driver: 'bar' } };
    dom::Object context;
    dom::Object is;
    is.set("a", "foo");
    context.set("is", is);
    dom::Object slave;
    slave.set("driver", "bar");
    context.set("slave", slave);

    HandlebarsOptions opt;
    opt.trackIds = true;

    // should not include anything without the flag
    {
        hbs.registerHelper("wycats", [](dom::Array const& args, HandlebarsCallback const& options) {
            BOOST_TEST(options.ids().empty());
            BOOST_TEST(options.hash().empty());
            return "success";
        });
        BOOST_TEST(hbs.render("{{wycats is.a slave.driver}}", context) == "success");
    }

    // should include argument ids
    {
        hbs.registerHelper("wycats", [](dom::Array const& args, HandlebarsCallback const& options) {
            BOOST_TEST(options.ids()[0].getString() == "is.a");
            BOOST_TEST(options.ids()[1].getString() == "slave.driver");
            std::string res = "HELP ME MY BOSS ";
            res += toString(options.ids()[0]);
            res += ":";
            res += toString(args[0]);
            res += " ";
            res += toString(options.ids()[1]);
            res += ":";
            res += toString(args[1]);
            return res;
        });
        BOOST_TEST(hbs.render("{{wycats is.a slave.driver}}", context, opt) == "HELP ME MY BOSS is.a:foo slave.driver:bar");
    }

    // should include hash ids
    {
        std::string string = "{{wycats bat=is.a baz=slave.driver}}";
        hbs.registerHelper("wycats", [](dom::Array const& args, HandlebarsCallback const& options) {
            BOOST_TEST(options.hashIds().find("bat").getString() == "is.a");
            BOOST_TEST(options.hashIds().find("baz").getString() == "slave.driver");
            std::string res = "HELP ME MY BOSS ";
            res += toString(options.hashIds().find("bat"));
            res += ":";
            res += toString(options.hash().find("bat"));
            res += " ";
            res += toString(options.hashIds().find("baz"));
            res += ":";
            res += toString(options.hash().find("baz"));
            return res;
        });
        BOOST_TEST(hbs.render(string, context, opt) == "HELP ME MY BOSS is.a:foo slave.driver:bar");
    }

    // should note ../ and ./ references
    {
        std::string string = "{{wycats ./is.a ../slave.driver this.is.a this}}";
        hbs.registerHelper("wycats", [](dom::Array const& args, HandlebarsCallback const& options) {
            BOOST_TEST(options.ids()[0].getString() == "is.a");
            BOOST_TEST(options.ids()[1].getString() == "../slave.driver");
            BOOST_TEST(options.ids()[2].getString() == "is.a");
            BOOST_TEST(options.ids()[3].getString() == "");
            std::string res = "HELP ME MY BOSS ";
            res += toString(options.ids()[0]);
            res += ":";
            res += toString(args[0]);
            res += " ";
            res += toString(options.ids()[1]);
            res += ":";
            res += toString(args[1]);
            return res;
        });
        // BOOST_TEST(hbs.render(string, context, opt) == "HELP ME MY BOSS is.a:foo ../slave.driver:undefined");
        BOOST_TEST(hbs.render(string, context, opt) == "HELP ME MY BOSS is.a:foo ../slave.driver:null");
    }

    // should note @data references
    {
        std::string string = "{{wycats @is.a @slave.driver}}";
        hbs.registerHelper("wycats", [](dom::Array const& args, HandlebarsCallback const& options) {
            BOOST_TEST(options.ids()[0].getString() == "@is.a");
            BOOST_TEST(options.ids()[1].getString() == "@slave.driver");
            std::string res = "HELP ME MY BOSS ";
            res += toString(options.ids()[0]);
            res += ":";
            res += toString(args[0]);
            res += " ";
            res += toString(options.ids()[1]);
            res += ":";
            res += toString(args[1]);
            return res;
        });
        opt.data = context;
        BOOST_TEST(hbs.render(string, context, opt) == "HELP ME MY BOSS @is.a:foo @slave.driver:bar");
        opt.data = nullptr;
    }

    // should return null for constants
    {
        std::string string = "{{wycats 1 \"foo\" key=false}}";
        hbs.registerHelper("wycats", [](dom::Array const& args, HandlebarsCallback const& options) {
            BOOST_TEST(options.ids()[0].isNull());
            BOOST_TEST(options.ids()[1].isNull());
            BOOST_TEST(options.hashIds().find("key").isNull());
            std::string res = "HELP ME MY BOSS ";
            res += toString(args[0]);
            res += " ";
            res += toString(args[1]);
            res += " ";
            res += toString(options.hash().find("key"));
            return res;
        });
        BOOST_TEST(hbs.render(string, context, opt) == "HELP ME MY BOSS 1 foo false");
    }

    // should return true for subexpressions
    {
        std::string string = "{{wycats (sub)}}";
        hbs.registerHelper("sub", [](dom::Array const& args, HandlebarsCallback const& options) {
            return 1;
        });
        hbs.registerHelper("wycats", [](dom::Array const& args, HandlebarsCallback const& options) {
            BOOST_TEST(options.ids()[0].getBool());
            return "HELP ME MY BOSS " + toString(args[0]);
        });
        BOOST_TEST(hbs.render(string, context, opt) == "HELP ME MY BOSS 1");
    }

    // should use block param paths
    {
        std::string string = "{{#doIt as |is|}}{{wycats is.a slave.driver is}}{{/doIt}}";
        hbs.registerHelper("doIt", [](dom::Array const& args, HandlebarsCallback const& options) {
            dom::Array blockParams;
            blockParams.emplace_back(options.context().getObject().find("is"));
            dom::Array blockParamPaths;
            blockParamPaths.emplace_back("zomg");
            return options.fn(options.context(), options.data(), blockParams, blockParamPaths);
        });
        hbs.registerHelper("wycats", [](dom::Array const& args, HandlebarsCallback const& options) {
            BOOST_TEST(options.ids()[0].getString() == "zomg.a");
            BOOST_TEST(options.ids()[1].getString() == "slave.driver");
            BOOST_TEST(options.ids()[2].getString() == "zomg");
            std::string res = "HELP ME MY BOSS ";
            res += toString(options.ids()[0]);
            res += ":";
            res += toString(args[0]);
            res += " ";
            res += toString(options.ids()[1]);
            res += ":";
            res += toString(args[1]);
            return res;
        });
        // context = { is: { a: 'foo' }, slave: { driver: 'bar' } };
        BOOST_TEST(hbs.render(string, context, opt) == "HELP ME MY BOSS zomg.a:foo slave.driver:bar");
    }

    hbs.registerHelper("blockParams", [](dom::Array const& args, HandlebarsCallback const& options) {
        std::string res = toString(args[0]);
        res += ":";
        res += toString(options.ids()[0]);
        res += "\n";
        return res;
    });
    hbs.registerHelper("wycats", [](dom::Array const& args, HandlebarsCallback const& options) {
        std::string res = toString(args[0]);
        res += ":";
        res += toString(options.data().find("contextPath"));
        res += "\n";
        return res;
    });

    // builtin helpers
    {
        // #each
        {
            // should track contextPath for arrays
            {
                dom::Object ctx;
                dom::Array array;
                dom::Object foo;
                foo.set("name", "foo");
                array.emplace_back(foo);
                dom::Object bar;
                bar.set("name", "bar");
                array.emplace_back(bar);
                ctx.set("array", array);
                BOOST_TEST(hbs.render("{{#each array}}{{wycats name}}{{/each}}", ctx, opt) == "foo:array.0\nbar:array.1\n");
            }

            // should track contextPath for keys
            {
                dom::Object ctx;
                dom::Object object;
                dom::Object foo;
                foo.set("name", "foo");
                object.set("foo", foo);
                dom::Object bar;
                bar.set("name", "bar");
                object.set("bar", bar);
                ctx.set("object", object);
                BOOST_TEST(hbs.render("{{#each object}}{{wycats name}}{{/each}}", ctx, opt) == "foo:object.foo\nbar:object.bar\n");
            }

            // should handle nesting
            {
                // { array: [{ name: 'foo' }, { name: 'bar' }] }
                dom::Object ctx;
                dom::Array array;
                dom::Object foo;
                foo.set("name", "foo");
                array.emplace_back(foo);
                dom::Object bar;
                bar.set("name", "bar");
                array.emplace_back(bar);
                ctx.set("array", array);
                BOOST_TEST(hbs.render("{{#each .}}{{#each .}}{{wycats name}}{{/each}}{{/each}}", ctx, opt) == "foo:.array..0\nbar:.array..1\n");
            }

            // should handle block params
            {
                // { array: [{ name: 'foo' }, { name: 'bar' }] }
                dom::Object ctx;
                dom::Array array;
                dom::Object foo;
                foo.set("name", "foo");
                array.emplace_back(foo);
                dom::Object bar;
                bar.set("name", "bar");
                array.emplace_back(bar);
                ctx.set("array", array);
                BOOST_TEST(hbs.render("{{#each array as |value|}}{{blockParams value.name}}{{/each}}", ctx, opt) == "foo:array.0.name\nbar:array.1.name\n");
                // TODO: Just implement this thing... there's no way around it.
            }
        }

        // #with
        {
            // should track contextPath
            {
                // { field: { name: 'foo' } }
                dom::Object ctx;
                dom::Object field;
                field.set("name", "foo");
                ctx.set("field", field);
                BOOST_TEST(hbs.render("{{#with field}}{{wycats name}}{{/with}}", ctx, opt) == "foo:field\n");
            }

            // should handle nesting
            {
                // { bat: { field: { name: 'foo' } } }
                dom::Object ctx;
                dom::Object bat;
                dom::Object field;
                field.set("name", "foo");
                bat.set("field", field);
                ctx.set("bat", bat);
                BOOST_TEST(hbs.render("{{#with bat}}{{#with field}}{{wycats name}}{{/with}}{{/with}}", ctx, opt) == "foo:bat.field\n");
            }
        }

        // #blockHelperMissing
        {
            // should track contextPath for arrays
            {
                std::string string = "{{#field}}{{wycats name}}{{/field}}";
                // { field: [{ name: 'foo' }] }
                dom::Object ctx;
                dom::Array array;
                dom::Object foo;
                foo.set("name", "foo");
                array.emplace_back(foo);
                ctx.set("field", array);
                BOOST_TEST(hbs.render(string, ctx, opt) == "foo:field.0\n");
            }

            // should track contextPath for keys
            {
                std::string string = "{{#field}}{{wycats name}}{{/field}}";
                // { field: { name: 'foo' } }
                dom::Object ctx;
                dom::Object field;
                field.set("name", "foo");
                ctx.set("field", field);
                BOOST_TEST(hbs.render(string, ctx, opt) == "foo:field\n");
            }

            // should handle nesting
            {
                std::string string = "{{#bat}}{{#field}}{{wycats name}}{{/field}}{{/bat}}";
                // { bat: { field: { name: 'foo' } } }
                dom::Object ctx;
                dom::Object bat;
                dom::Object field;
                field.set("name", "foo");
                bat.set("field", field);
                ctx.set("bat", bat);
                BOOST_TEST(hbs.render(string, ctx, opt) == "foo:bat.field\n");
            }
        }
    }

    // partials
    {
        // should pass track id for basic partial
        {
            std::string string = "Dudes: {{#dudes}}{{> dude}}{{/dudes}}";
            dom::Object ctx;
            dom::Array dudes;
            dom::Object yehuda;
            yehuda.set("name", "Yehuda");
            yehuda.set("url", "http://yehuda");
            dudes.emplace_back(yehuda);
            dom::Object alan;
            alan.set("name", "Alan");
            alan.set("url", "http://alan");
            dudes.emplace_back(alan);
            ctx.set("dudes", dudes);
            hbs.registerPartial("dude", "{{wycats name}}");
            BOOST_TEST(hbs.render(string, ctx, opt) == "Dudes: Yehuda:dudes.0\nAlan:dudes.1\n");
        }

        // should pass track id for context partial
        {
            std::string string = "Dudes: {{> dude dudes}}";
            // { dudes: [ { name: 'Yehuda', url: 'http://yehuda' }, { name: 'Alan', url: 'http://alan' } ] }
            dom::Object ctx;
            dom::Array dudes;
            dom::Object yehuda;
            yehuda.set("name", "Yehuda");
            yehuda.set("url", "http://yehuda");
            dudes.emplace_back(yehuda);
            dom::Object alan;
            alan.set("name", "Alan");
            alan.set("url", "http://alan");
            dudes.emplace_back(alan);
            ctx.set("dudes", dudes);
            hbs.registerPartial("dude", "{{#each this}}{{wycats name}}{{/each}}");
            BOOST_TEST(hbs.render(string, ctx, opt) == "Dudes: Yehuda:dudes..0\nAlan:dudes..1\n");
        }

        // should invalidate context for partials with parameters
        {
            std::string string = "Dudes: {{#dudes}}{{> dude . bar=\"foo\"}}{{/dudes}}";
            dom::Object ctx;
            dom::Array dudes;
            dom::Object yehuda;
            yehuda.set("name", "Yehuda");
            yehuda.set("url", "http://yehuda");
            dudes.emplace_back(yehuda);
            dom::Object alan;
            alan.set("name", "Alan");
            alan.set("url", "http://alan");
            dudes.emplace_back(alan);
            ctx.set("dudes", dudes);
            hbs.registerPartial("dude", "{{wycats name}}");
            BOOST_TEST(hbs.render(string, ctx, opt) == "Dudes: Yehuda:true\nAlan:true\n");
        }
    }
}

void
strict()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/strict.js
    Handlebars hbs;

    HandlebarsOptions opt;
    opt.strict = true;

    // should error on missing property lookup
    {
        BOOST_TEST_THROW_WITH(
            hbs.render("{{hello}}", dom::Object{}, opt),
            HandlebarsError, "\"hello\" not defined in [object Object] - 1:2");
    }

    // should error on missing child
    {
        // { hello: { bar: 'foo' } }
        dom::Object ctx;
        dom::Object hello;
        hello.set("bar", "foo");
        ctx.set("hello", hello);
        BOOST_TEST(hbs.render("{{hello.bar}}", ctx, opt) == "foo");

        // { hello: {} }
        ctx.set("hello", dom::Object());
        BOOST_TEST_THROW_WITH(
            hbs.render("{{hello.bar}}", ctx, opt),
            HandlebarsError, "\"bar\" not defined in [object Object] - 1:8");
    }

    // should handle explicit undefined
    {
        // { hello: { bar: undefined } }
        dom::Object ctx;
        dom::Object hello;
        hello.set("bar", nullptr);
        ctx.set("hello", hello);
        BOOST_TEST(hbs.render("{{hello.bar}}", ctx, opt) == "");
    }

    // should error on missing property lookup in known helpers mode
    {
        BOOST_TEST_THROW_WITH(
            hbs.render("{{hello}}", dom::Object{}, opt),
            HandlebarsError, "\"hello\" not defined in [object Object] - 1:2");
    }

    // should error on missing context
    {
        BOOST_TEST_THROW_WITH(
            hbs.render("{{hello}}", dom::Object{}, opt),
            HandlebarsError, "\"hello\" not defined in [object Object] - 1:2");
    }

    // should error on missing data lookup
    {
        std::string string = "{{@hello}}";
        BOOST_TEST_THROW_WITH(
            hbs.render(string, dom::Object{}, opt),
            HandlebarsError, "\"hello\" not defined in [object Object] - 1:3");
        dom::Object data;
        data.set("hello", "foo");
        opt.data = data;
        BOOST_TEST(hbs.render(string, dom::Object{}, opt) == "foo");
        opt.data = nullptr;
    }

    // should not run helperMissing for helper calls
    {
        std::string string = "{{hello foo}}";
        dom::Object ctx;
        ctx.set("foo", true);
        BOOST_TEST_THROW_WITH(
            hbs.render(string, ctx, opt),
            HandlebarsError, "\"hello\" not defined in [object Object] - 1:2");

        string = "{{#hello foo}}{{/hello}}";
        BOOST_TEST_THROW_WITH(
            hbs.render(string, ctx, opt),
            HandlebarsError, "\"hello\" not defined in [object Object] - 1:3");
    }

    // should throw on ambiguous blocks
    {
        BOOST_TEST_THROW_WITH(
            hbs.render("{{#hello}}{{/hello}}", dom::Object{}, opt),
            HandlebarsError, "\"hello\" not defined in [object Object] - 1:3");

        BOOST_TEST_THROW_WITH(
            hbs.render("{{^hello}}{{/hello}}", dom::Object{}, opt),
            HandlebarsError, "\"hello\" not defined in [object Object] - 1:3");

        dom::Object ctx;
        ctx.set("hello", dom::Object());
        BOOST_TEST_THROW_WITH(
            hbs.render("{{#hello.bar}}{{/hello.bar}}", ctx, opt),
            HandlebarsError, "\"bar\" not defined in [object Object] - 1:9");
    }

    // should allow undefined parameters when passed to helpers
    {
        BOOST_TEST(hbs.render("{{#unless foo}}success{{/unless}}", dom::Object{}, opt) == "success");
    }

    // should allow undefined hash when passed to helpers
    {
        std::string string = "{{helper value=@foo}}";
        hbs.registerHelper("helper", [](dom::Array const& args, HandlebarsCallback const& options) {
            BOOST_TEST(options.hash().exists("value"));
            BOOST_TEST(options.hash().find("value").isNull());
            return "success";
        });
        BOOST_TEST(hbs.render(string, dom::Object{}, opt) == "success");
    }

    // should show error location on missing property lookup
    {
        std::string string = "\n\n\n   {{hello}}";
        BOOST_TEST_THROW_WITH(
            hbs.render(string, dom::Object{}, opt),
            HandlebarsError, "\"hello\" not defined in [object Object] - 4:5");
    }
}

void
assume_objects()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/strict.js
    Handlebars hbs;

    HandlebarsOptions assumeOpt;
    assumeOpt.assumeObjects = true;

    // should ignore missing property
    {
        BOOST_TEST(hbs.render("{{hello}}", dom::Object{}, assumeOpt) == "");
    }

    // should ignore missing child
    {
        dom::Object ctx;
        ctx.set("hello", dom::Object());
        BOOST_TEST(hbs.render("{{hello.bar}}", ctx, assumeOpt) == "");
    }

    // should error on missing object
    {
        BOOST_TEST_THROW_WITH(
            hbs.render("{{hello.bar}}", dom::Object{}, assumeOpt),
            HandlebarsError, "\"hello\" not defined in [object Object] - 1:2");
    }

    // should error on missing context
    {
        dom::Value ctx(nullptr);
        BOOST_TEST_THROW_WITH(
            hbs.render("{{hello}}", ctx, assumeOpt),
            HandlebarsError, "\"hello\" not defined in null - 1:2");
    }

    // should error on missing data lookup
    {
        dom::Value ctx(nullptr);
        BOOST_TEST_THROW_WITH(
            hbs.render("{{@hello.bar}}", ctx, assumeOpt),
            HandlebarsError, "\"hello\" not defined in [object Object] - 1:3");
    }

    // should execute blockHelperMissing
    {
        BOOST_TEST(hbs.render("{{^hello}}foo{{/hello}}", dom::Object{}, assumeOpt) == "foo");
    }
}

void
utils()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/utils.js
    Handlebars hbs;

    // SafeString
    {
        // it should not escape SafeString properties
        {
            dom::Value name = safeString("<em>Sean O&#x27;Malley</em>");
            dom::Object ctx;
            ctx.set("name", name);
            BOOST_TEST(hbs.render("{{name}}", ctx) == "<em>Sean O&#x27;Malley</em>");
        }
    }

    // escapeExpression
    {
        // should escape html
        {
            BOOST_TEST(escapeExpression("foo<&\"'>") == "foo&lt;&amp;&quot;&#x27;&gt;");
            BOOST_TEST(escapeExpression("foo=") == "foo&#x3D;");
        }

        // should not escape SafeString
        {
            dom::Value string = safeString("foo<&\"'>");
            BOOST_TEST(escapeExpression(string) == "foo<&\"'>");

            dom::Object ctx;
            ctx.set("toHTML", dom::makeInvocable([]() {
                return "foo<&\"'>";
            }));
            BOOST_TEST(escapeExpression(ctx) == "foo<&\"'>");
        }

        // should handle falsy
        {
            BOOST_TEST(escapeExpression("").empty());
            BOOST_TEST(escapeExpression(dom::Value(dom::Kind::Undefined)).empty());
            BOOST_TEST(escapeExpression(dom::Value(dom::Kind::Null)).empty());
            BOOST_TEST(escapeExpression(false) == "false");
            BOOST_TEST(escapeExpression(0) == "0");
        }
    }

    // isEmpty
    {
        // should be empty
        BOOST_TEST(isEmpty(dom::Value(dom::Kind::Undefined)));
        BOOST_TEST(isEmpty(dom::Value(dom::Kind::Null)));
        BOOST_TEST(isEmpty(false));
        BOOST_TEST(isEmpty(""));
        BOOST_TEST(isEmpty(dom::Array()));

        // should not be empty
        BOOST_TEST(!isEmpty(0));
        BOOST_TEST(!isEmpty(dom::Array({1})));
        BOOST_TEST(!isEmpty("foo"));
        dom::Object ctx;
        ctx.set("bar", 1);
        BOOST_TEST(!isEmpty(ctx));
    }
}

void
mustache_compat_spec()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/spec.js
}

void
run()
{
    master_test();
    safe_string();
    basic_context();
    whitespace_control();
    partials();
    partial_blocks();
    inline_partials();
    standalone_partials();
    partial_compat_mode();
    blocks();
    block_inverted_sections();
    block_standalone_sections();
    block_compat_mode();
    block_decorators();
    subexpressions();
    builtin_if();
    builtin_with();
    builtin_each();
    builtin_log();
    builtin_lookup();
    data();
    helpers();
    track_ids();
    strict();
    assume_objects();
    utils();
    mustache_compat_spec();
}

};

TEST_SUITE(
    Handlebars_test,
    "clang.mrdox.Handlebars");

} // mrdox
} // clang


