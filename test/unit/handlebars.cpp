//
// Copyright (c) 2023 alandefreitas (alandefreitas@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt
//

#include <mrdox/Support/Handlebars.hpp>
#include <mrdox/Support/Path.hpp>
#include "test_macros.hpp"
#include <ranges>

using namespace clang::mrdox;

namespace {
    // Helpers from the handlebars documentation
    llvm::json::Value
    loud_fn(std::string_view arg) {
        return llvm::StringRef(arg).upper();
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
            return "progress helper requires string argument";
        }
        if (args[1].kind() != llvm::json::Value::Kind::Number) {
            return "progress helper requires number argument";
        }
        if (args[2].kind() != llvm::json::Value::Kind::Boolean) {
            return "progress helper requires boolean argument";
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
}


int
main() {
    auto fileTextExp = files::getFileText(MRDOX_UNIT_TEST_DIR "/fixtures/function.adoc.hbs");
    REQUIRE(fileTextExp);
    auto template_str = *fileTextExp;
    REQUIRE_FALSE(template_str.empty());

    HandlebarsOptions options;
    options.noEscape = true;

    // Create context for tests
    llvm::json::Object context;
    llvm::json::Object page;
    page["kind"] = "record";
    page.try_emplace("kind", "record");
    page.try_emplace("name", "from_chars");
    page.try_emplace("decl", "std::from_chars");
    page.try_emplace("loc", "charconv");
    llvm::json::Object javadoc;
    javadoc.try_emplace("brief", "Converts strings to numbers");
    javadoc.try_emplace("details", "This function converts strings to numbers");
    page.try_emplace("javadoc", std::move(javadoc));
    page.try_emplace("synopsis", "This is the from_chars function");
    llvm::json::Object person;
    person.try_emplace("firstname", "John");
    person.try_emplace("lastname", "Doe");
    page.try_emplace("person", std::move(person));
    llvm::json::Array people;
    auto first_and_last_names = {
            std::make_pair("Alice", "Doe"),
            std::make_pair("Bob", "Doe"),
            std::make_pair("Carol", "Smith")};
    for (auto [firstname, lastname]: first_and_last_names) {
        person = {};
        person.try_emplace("firstname", firstname);
        person.try_emplace("lastname", lastname);
        people.push_back(std::move(person));
    }
    page.try_emplace("people", std::move(people));
    page.try_emplace("specialChars", "& < > \" ' ` =");
    page.try_emplace("url", "https://cppalliance.org/");
    context.try_emplace("page", std::move(page));

    // Register some extra test helpers
    Handlebars hbs;
    helpers::registerStandardHelpers(hbs);
    helpers::registerAntoraHelpers(hbs);
    hbs.registerHelper("progress", progress_fn);
    hbs.registerHelper("link", link_fn);
    hbs.registerHelper("loud", loud_fn);
    hbs.registerHelper("to_string", to_string_fn);

    // Render template with all handlebars features
    std::string r = hbs.render(template_str, context, options);
    REQUIRE_FALSE(r.empty());

    fmt::println("All tests passed!");
    fmt::println("Parsed template: {}", r);
    return EXIT_SUCCESS;
}
