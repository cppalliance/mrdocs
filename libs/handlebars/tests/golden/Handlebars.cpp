//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "../HandlebarsTestSupport.hpp"
#include <test_suite/detail/decomposer.hpp>
#include <test_suite/diff.hpp>
#include <test_suite/test_suite.hpp>
#include <llvm/Support/JSON.h>
#include <llvm/Support/MemoryBuffer.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

// Handlebars golden/reference tests: render fixture templates and compare to
// reference output, and run the mustache spec JSON fixtures. Uses LLVM for
// JSON parsing and file reading; only this executable pulls LLVM in.

namespace mrdocs {

namespace {

// Local file helpers so this test depends only on mrdocs::handlebars + the test
// framework + LLVM (no mrdocs/Support). Read a file fully, or report the
// filename component of a path.
std::optional<std::string>
readFileText(std::string_view path)
{
    std::ifstream in{std::filesystem::path(path), std::ios::binary};
    if (!in)
    {
        return std::nullopt;
    }
    return std::string(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
}

std::string
fileName(std::string_view path)
{
    return std::filesystem::path(path).filename().string();
}

} // (anonymous)

struct Handlebars_golden_test
{

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
        MRDOCS_TEST_FILES_DIR "/features_test.adoc.hbs";
    master.partial_paths = {
        MRDOCS_TEST_FILES_DIR "/record-detail.adoc.hbs",
        MRDOCS_TEST_FILES_DIR "/record.adoc.hbs",
        MRDOCS_TEST_FILES_DIR "/escaped.adoc.hbs"};
    master.output_path =
        MRDOCS_TEST_FILES_DIR "/features_test.adoc";
    master.error_output_path =
        MRDOCS_TEST_FILES_DIR "/features_test_error.adoc";
    master.logger_output_path =
        MRDOCS_TEST_FILES_DIR "/logger_output.txt";
    master.logger_error_output_path =
        MRDOCS_TEST_FILES_DIR "/logger_output_error.txt";

    auto template_text_r =
        readFileText(master.template_path);
    BOOST_TEST(template_text_r);
    master.template_str = *template_text_r;
    BOOST_TEST_NOT(master.template_str.empty());

    auto master_file_contents_r =
        readFileText(master.output_path);
    if (master_file_contents_r)
    {
        master.master_file_contents = *master_file_contents_r;
    }

    auto master_logger_output_r =
        readFileText(master.logger_output_path);
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
    dom::Object doc;
    doc.set("brief", "Converts strings to numbers");
    doc.set("details", "This function converts strings to numbers");
    page.set("doc", doc);
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

    master.hbs.registerHelper("progress", dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::size_t const n = arguments.size();
        if (n < 4)
        {
          return std::format(
              "progress helper requires 3 arguments: {} provided",
              arguments.size());
        }
        if (!arguments.get(0).isString())
        {
          return std::format(
              "progress helper requires string argument: {} received",
              arguments.get(0));
        }
        if (!arguments.get(1).isInteger())
        {
          return std::format(
              "progress helper requires number argument: {} received",
              arguments.get(1));
        }
        if (!arguments.get(2).isBoolean())
        {
          return std::format(
              "progress helper requires boolean argument: {} received",
              arguments.get(2));
        }
        dom::Value nameV = arguments.get(0);
        std::string_view name = nameV.getString();
        std::uint64_t percent = arguments.get(1).getInteger();
        bool stalled = arguments.get(2).getBool();
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
    }));

    static auto noop_fn = dom::makeVariadicInvocable(
        [](dom::Array const& arguments) -> dom::Value
    {
        dom::Value options = arguments.back();
        if (options.get("fn"))
        {
            // If the hook is not overridden, then the default implementation will
            // mimic the behavior of Mustache and just render the block.
            options.get("write")(options.get("context"));
            return {};
        }
        if (arguments.size() > 1)
        {
          return std::format(R"(Missing helper: "{}")", options.get("name"));
        }
        return {};
    });

    master.hbs.registerHelper("noop", noop_fn);
    master.hbs.registerHelper("raw", noop_fn);

    master.hbs.registerHelper("link", dom::makeVariadicInvocable([](
        dom::Array const& args) -> std::string {
        if (args.size() < 2)
        {
            return "no arguments provided to link helper";
        }
        std::size_t const n = args.size();
        for (std::size_t i = 0; i < n - 1; ++i)
        {
            if (!args.get(i).isString())
            {
              return std::format(
                  "link helper requires string arguments: {} provided",
                  args.size());
            }
        }

        std::string out;
        dom::Value options = args.back();
        dom::Value hash = options.get("hash");
        auto h = hash.get("href");
        if (h.isString())
        {
            out += h.getString();
        }
        else if (args.size() > 1)
        {
            if (!args.get(1).isString())
            {
              return std::format(
                  "link helper requires string argument: {} provided",
                  toString(args.get(1).kind()));
            }
            auto href = args.get(1);
            out += href.getString();
        }
        else
        {
            out += "#";
        }

        out += '[';
        out += args.get(0).getString();

        // more attributes from hashes
        if (hash)
        {
            dom::Object const& hashObj = hash.getObject();
            hashObj.visit([&](dom::String const& key, dom::Value const& value)
            {
                if (key == "href" || !value.isString())
                {
                    return true;
                }
                out += ',';
                out += key;
                out += '=';
                out += value.getString();
                return true;
            });
        }
        out += ']';

        return out;
    }));

    master.hbs.registerHelper("loud", dom::makeVariadicInvocable([](
        dom::Array const& args) -> std::string
    {
        std::string res;
        dom::Value options = args.back();
        dom::Value fn = options.get("fn");
        if (fn.isFunction())
        {
            res = static_cast<std::string>(fn());
        }
        else
        {
            if (args.size() < 2)
            {
                return "loud helper requires at least one argument";
            }
            dom::Array::value_type const& firstArg = args.get(0);
            if (!firstArg.isString())
            {
              return std::format(
                  "loud helper requires string argument: {} provided",
                  toString(firstArg.kind()));
            }
            res = firstArg.getString();
        }
        for (char& c : res)
        {
            if (c >= 'a' && c <= 'z')
                c += 'A' - 'a';
        }
        return res;
    }));

    master.hbs.registerHelper("to_string", [](
        dom::Value const& arg) -> std::string {
        return dom::JSON::stringify(arg);
    });

    master.hbs.registerHelper("bold", dom::makeVariadicInvocable([](
        dom::Array const& args) {
        dom::Value options = args.back();
        return std::format(R"(<div class="mybold">{}</div>)",
                           options.get("fn")());
    }));

    master.hbs.registerHelper("list", dom::makeVariadicInvocable([](
        dom::Array const& args) -> dom::Value {
        // Built-in helper to change the context for each object in args
        if (args.size() < 2)
        {
          return std::format("list helper requires 1 argument: {} provided",
                             args.size() - 1);
        }
        if (!args.get(0).isArray())
        {
          return std::format("list helper requires array argument: {} provided",
                             toString(args.get(0).kind()));
        }

        dom::Value options = args.back();
        dom::Object data = createFrame(options.get("data"));
        dom::Value itemsV = args.get(0);
        dom::Array const& items = itemsV.getArray();
        if (!items.empty())
        {
            std::string out = "<ul";
            dom::Value hash = options.get("hash");
            hash.getObject().visit([&](dom::String const& key, dom::Value const& value)
            {
                out += " ";
                out += key;
                out += "=\"";
                out += value.getString();
                out += "\"";
            });
            out += ">";
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                dom::Value item = items.get(i);
                data.set("key", static_cast<std::int64_t>(i));
                data.set("first", i == 0);
                data.set("last", i == items.size() - 1);
                data.set("index", static_cast<std::int64_t>(i));
                dom::Object fn_options;
                fn_options.set("data", data);
                out += toString("<li>" + options.get("fn")(item, fn_options) + "</li>");
            }
            return out + "</ul>";
        }
        return options.get("inverse")();
    }));

    master.hbs.registerHelper("isdefined",
        [](dom::Value const& val) -> dom::Value
    {
        return !val.isUndefined();
    });

    master.hbs.registerHelper("helperMissing",
        dom::makeVariadicInvocable([](
        dom::Array const& args)
    {
        dom::Value options = args.back();
        std::string out;
        out += "Missing: ";
        out += toString(options.get("name"));
        out += "(";
        std::size_t const n = args.size();
        for (std::size_t i = 0; i < n - 1; ++i) {
            if (i != 0)
            {
                out += ", ";
            }
            out += toString(args.get(i));
        }
        out += ")";
        return out;
    }));

    master.hbs.registerHelper("blockHelperMissing",
        dom::makeVariadicInvocable([](
        dom::Array const& args)
    {
        std::string out;
        OutputRef os(out);
        os << "Helper '";
        dom::Value options = args.back();
        os << options.get("name");
        os << "' not found. Printing block: ";
        os << options.get("fn")();
        return out;
    }));
}

void
setup_logger()
{
    master.hbs.registerLogger(
        dom::makeVariadicInvocable([this](
        dom::Array const& args)
    {
        dom::Value level = args.get(0);
        master.log += std::format("[{}] ", level);
        for (std::size_t i = 1; i < args.size(); ++i)
        {
            if (i != 1)
            {
                master.log += ", ";
            }
            master.log += args.get(i).getString();
        }
        master.log += '\n';
    }));
}

void
setup_partials()
{
    // From files
    for (auto partial_path: master.partial_paths)
    {
        auto partial_text_r = readFileText(partial_path);
        BOOST_TEST(partial_text_r);
        std::string filename = fileName(partial_path);
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

static
dom::Value
to_dom(llvm::json::Value& val)
{
    dom::Value res;
    // val is llvm::json::Object
    llvm::json::Object* obj_ptr = val.getAsObject();
    if (obj_ptr)
    {
        dom::Object obj;
        auto it = obj_ptr->begin();
        while (it != obj_ptr->end())
        {
            obj.set(it->first.str(), to_dom(it->second));
            ++it;
        }
        res = obj;
        return res;
    }

    // val is array
    llvm::json::Array* arr_ptr = val.getAsArray();
    if (arr_ptr)
    {
        dom::Array arr;
        for (auto& item: *arr_ptr)
        {
            arr.emplace_back(to_dom(item));
        }
        res = arr;
        return res;
    }

    // val is string
    std::optional<llvm::StringRef> str_opt = val.getAsString();
    if (str_opt) {
        return str_opt.value().str();
    }

    // val is integer
    std::optional<std::int64_t> int_opt = val.getAsInteger();
    if (int_opt) {
        return int_opt.value();
    }

    // val is double (convert to string)
    std::optional<double> num_opt = val.getAsNumber();
    if (num_opt) {
        std::string double_str = std::to_string(num_opt.value());
        double_str.erase(double_str.find_last_not_of('0') + 1, std::string::npos);
        return double_str;
    }

    // val is bool
    std::optional<bool> bool_opt = val.getAsBoolean();
    if (bool_opt) {
        return bool_opt.value();
    }

    return res;
};

void
mustache_compat_spec()
{
    // https://github.com/handlebars-lang/handlebars.js/blob/4.x/spec/spec.js
    std::string_view mustache_specs_dir =
        MRDOCS_TEST_FILES_DIR "/mustache/";
    std::vector<std::string> spec_files;
    for (auto& p: std::filesystem::directory_iterator(mustache_specs_dir))
    {
        if (p.is_regular_file())
        {
            spec_files.emplace_back(p.path().filename().string());
        }
    }

    for (auto const& spec_file: spec_files) {
        // Skip mustache extensions (handlebars knowingly deviates from these)
        if (spec_file.starts_with('~'))
        {
            continue;
        }

        // Load JSON file
        std::string spec_path = std::string(mustache_specs_dir) + std::string(spec_file);
        llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> FileOrErr =
            llvm::MemoryBuffer::getFile(spec_path, true);
        BOOST_TEST(FileOrErr);
        std::unique_ptr<llvm::MemoryBuffer> Buffer = std::move(*FileOrErr);

        // Parse the JSON content
        llvm::Expected<llvm::json::Value> jsonObj =
            llvm::json::parse(Buffer->getBuffer());
        BOOST_TEST(jsonObj);
        llvm::json::Value jsonData = std::move(*jsonObj);
        BOOST_TEST(jsonData.getAsObject());
        llvm::json::Object data = std::move(*jsonData.getAsObject());

        // Iterate tests
        llvm::json::Array tests = std::move(*data.get("tests")->getAsArray());
        for (auto testPtr: tests) {
            llvm::json::Object test = *testPtr.getAsObject();
            // Skip invalid partial tests
            llvm::StringRef test_name =
                *test.get("name")->getAsString();
            if (
                // Handlebars throws if partials are not found
                (spec_file == "partials.json" && test_name == "Failed Lookup") ||
                // Handlebars nests the entire response from partials, not just the literals
                (spec_file == "partials.json" && test_name == "Standalone Indentation"))
            {
                continue;
            }

            // Get template
            std::string template_str =test.get("template")->getAsString()->str();
            if (template_str.find("{{=") != std::string::npos)
            {
                // "{{=" not supported by handlebars
                continue;
            }

            // Get partials
            std::vector<std::pair<std::string, std::string>> partials;
            llvm::json::Value* partialsPtr = test.get("partials");
            llvm::json::Object* partialsObj = partialsPtr ? partialsPtr->getAsObject() : nullptr;
            if (partialsObj)
            {
                auto it = partialsObj->begin();
                bool incompatiblePartial = false;
                while (it != partialsObj->end())
                {
                    llvm::StringRef partial_string = *it->second.getAsString();
                    if (partial_string.find("{{=") != llvm::StringRef::npos)
                    {
                        // "{{=" not supported by handlebars
                        incompatiblePartial = true;
                        break;
                    }
                    else
                    {
                        partials.emplace_back(it->first.str(), partial_string.str());
                    }
                    ++it;
                }
                if (incompatiblePartial) {
                    continue;
                }
            }

            // Render
            Handlebars hbs;
            for (auto [name, partial]: partials)
            {
                hbs.registerPartial(name, partial);
            }
            dom::Value context = to_dom(*test.get("data"));
            HandlebarsOptions opt;
            opt.compat = true;
            std::string expected = test.get("expected")->getAsString().value().str();
            std::string rendered = hbs.render(template_str, context, opt);
            if (!BOOST_TEST(rendered == expected))
            {
                return;
            }
        }
    }
}

void
run()
{
    master_test();
    mustache_compat_spec();
}

};

TEST_SUITE(
    Handlebars_golden_test,
    "clang.mrdocs.handlebars.golden");

} // mrdocs

int
main(int argc, char const** argv)
{
    return test_suite::unit_test_main(argc, argv);
}
