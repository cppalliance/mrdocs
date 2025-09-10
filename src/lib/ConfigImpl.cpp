//
// This is a derivative work. originally part of the LLVM Project.
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Vinnie Falco (vinnie.falco@gmail.com)
// Copyright (c) 2023 Krystian Stasiowski (sdkrystian@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include "ConfigImpl.hpp"
#include "lib/Support/Glob.hpp"
#include "lib/Support/Path.hpp"
#include <mrdocs/Support/Path.hpp>
#include <mrdocs/Support/Concepts.hpp>
#include <clang/Tooling/AllTUsExecution.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/YAMLParser.h>
#include <llvm/Support/YAMLTraits.h>
#include <utility>

namespace clang {
namespace mrdocs {

namespace {

dom::Object
toDomObject(std::string_view configYaml);

} // (anon)

ConfigImpl::
ConfigImpl(access_token, ThreadPool& threadPool)
    : threadPool_(threadPool)
{}

//------------------------------------------------

bool
ConfigImpl::
shouldVisitSymbol(StringRef filePath) const noexcept
{
    if (settings_.input.empty())
    {
        return true;
    }
    for (auto& p: settings_.input)
    {
        // Exact match
        if (filePath == p)
        {
            return true;
        }
        // Prefix match
        if (filePath.starts_with(p))
        {
            bool const validPattern = std::ranges::any_of(
                    settings_.filePatterns,
                    [&](PathGlobPattern const &pattern) {
                        return pattern.match(filePath);
                    });
            if (validPattern)
            {
                return true;
            }
        }
    }
    return false;
}

bool
ConfigImpl::
shouldExtractFromFile(
    StringRef filePath,
    std::string& prefixPath) const noexcept
{
    namespace path = llvm::sys::path;

    SmallPathString temp;
    if(! files::isAbsolute(filePath))
    {
        temp = files::makePosixStyle(
            files::makeAbsolute(filePath, settings_.configDir()));
    }
    else
    {
        temp = filePath;
    }
    if(! path::replace_path_prefix(temp,
            settings_.sourceRoot, "", path::Style::posix))
        return false;
    MRDOCS_ASSERT(files::isDirsy(settings_.sourceRoot));
    prefixPath.assign(
        settings_.sourceRoot.begin(),
        settings_.sourceRoot.end());
    return true;
}

//------------------------------------------------

Expected<std::shared_ptr<ConfigImpl const>>
ConfigImpl::
load(
    Settings const& publicSettings,
    ReferenceDirectories const& dirs,
    ThreadPool& threadPool)
{
    auto c = std::make_shared<ConfigImpl>(access_token{}, threadPool);
    MRDOCS_ASSERT(c);

    // Validate and copy input settings
    SettingsImpl& s = c->settings_;
    dynamic_cast<Settings&>(s) = publicSettings;
    MRDOCS_TRY(Config::Settings::load(s, "", dirs));
    s.configYaml = publicSettings.configYaml;

    // Config strings
    c->configObj_ = toDomObject(s.configYaml);
    c->settings_.visit([&c]<class T>(std::string_view name, T& value) {
        if (!c->configObj_.exists(name))
        {
            if constexpr (std::convertible_to<T, std::string_view>)
            {
                c->configObj_.set(name, std::string(value));
            }
            else if constexpr (range_of_tuple_like<T>)
            {
                dom::Object obj;
                auto keys = value | std::views::keys;
                auto vals = value | std::views::values;
                auto zip = std::views::zip(keys, vals);
                for (auto const& [k, v] : zip)
                {
                    obj.set(k, v);
                }
                c->configObj_.set(name, std::move(obj));
            }
            else if constexpr (std::ranges::range<T>)
            {
                using ValueType = std::ranges::range_value_t<T>;
                dom::Array arr;
                for (auto const& v : value)
                {
                    if constexpr (
                        std::is_same_v<ValueType, PathGlobPattern> ||
                        std::is_same_v<ValueType, SymbolGlobPattern>)
                    {
                        arr.emplace_back(v.pattern());
                    }
                    else
                    {
                        arr.emplace_back(v);
                    }
                }
                c->configObj_.set(name, std::move(arr));
            }
            else if constexpr (std::is_enum_v<T>)
            {
                c->configObj_.set(name, to_string(value));
            }
            else
            {
                c->configObj_.set(name, value);
            }
        }
    });
    return c;
}

namespace {
dom::Value
toDom(llvm::yaml::Node* Value);

dom::Object
toDomObject(llvm::yaml::MappingNode* Object)
{
    dom::Object obj;
    for (auto &Pair : *Object)
    {
        auto *KeyString = dyn_cast<llvm::yaml::ScalarNode>(Pair.getKey());
        if (!KeyString) { continue; }
        SmallString<10> KeyStorage;
        StringRef KeyValue = KeyString->getValue(KeyStorage);
        llvm::yaml::Node *Value = Pair.getValue();
        if (!Value) {
            obj.set(KeyValue, dom::Kind::Undefined);
            continue;
        }
        dom::Value value = toDom(Value);
        obj.set(KeyValue, value);
    }
    return obj;
}

dom::Array
toDomArray(llvm::yaml::SequenceNode* Array)
{
    dom::Array arr;
    for (auto &Node : *Array)
    {
        dom::Value value = toDom(&Node);
        arr.push_back(value);
    }
    return arr;
}

dom::Value
toDomScalar(llvm::yaml::ScalarNode* Scalar)
{
    SmallString<10> ScalarStorage;
    StringRef ScalarValue = Scalar->getValue(ScalarStorage);
    StringRef RawValue = Scalar->getRawValue();
    bool const isEscaped = RawValue.size() != ScalarValue.size();
    if (isEscaped)
    {
        return ScalarValue;
    }
    std::int64_t integer;
    auto res = std::from_chars(
        ScalarValue.begin(),
        ScalarValue.end(),
        integer);
    if (res.ec == std::errc())
    {
        return integer;
    }
    bool const isBool = ScalarValue == "true" || ScalarValue == "false";
    if (isBool)
    {
        return ScalarValue == "true";
    }
    bool const isNull = ScalarValue == "null";
    if (isNull)
    {
        return nullptr;
    }
    return ScalarValue;
}

dom::Value
toDom(llvm::yaml::Node* Value)
{
    auto *ValueObject = dyn_cast<llvm::yaml::MappingNode>(Value);
    if (ValueObject)
    {
        return toDomObject(ValueObject);
    }
    auto *ValueArray = dyn_cast<llvm::yaml::SequenceNode>(Value);
    if (ValueArray)
    {
        return toDomArray(ValueArray);
    }
    auto *ValueString = dyn_cast<llvm::yaml::ScalarNode>(Value);
    if (ValueString)
    {
        return toDomScalar(ValueString);
    }
    return nullptr;
}

/* Convert a YAML string to a DOM object.

   YAML forbids tab characters to use as indentation so
   only some JSON files are valid YAML.

   Also instead of providing built-in support for
   types such as `bool` or `int`, YAML uses strings
   for everything, which the specification defines
   as "scalar" values.

   When converting a scalar to a DOM value, only
   escaped strings are preserved as strings.
   Unescaped strings are converted to numbers
   if possible, and then to booleans if possible.
   This is done to preserve compatibility with
   JSON, allow the user to specify scalars as
   boolean or integer values, match the original
   intent of the author, and for scalar values
   to interoperate with other handlebars templates.

 */
dom::Object
toDomObject(std::string_view yaml)
{
    llvm::SourceMgr SM;
    llvm::yaml::Stream YAMLStream_(yaml, SM);
    llvm::yaml::document_iterator I = YAMLStream_.begin();
    if (I == YAMLStream_.end())
    {
        return {};
    }
    llvm::yaml::Node *Root = I->getRoot();
    auto *Object = dyn_cast<llvm::yaml::MappingNode>(Root);
    if (!Object)
    {
        return {};
    }
    return toDomObject(Object);
}
} // (anon)

dom::Object const&
ConfigImpl::object() const {
    return configObj_;
}

} // mrdocs
} // clang
