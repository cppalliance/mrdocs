//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//

#include <mrdocs/Handlebars/Helpers/Container.hpp>
#include <mrdocs/Handlebars.hpp>
#include <mrdocs/Handlebars/Helpers/detail/Sequence.hpp>
#include <mrdocs/Handlebars/Helpers/detail/KeyPath.hpp>
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mrdocs {
namespace handlebars {
namespace helpers {

// Impl fragment of Handlebars.cpp (one translation unit); included within
// `namespace mrdocs { namespace helpers {`. Not a standalone header.

namespace container_helpers_detail {

/* Shared match predicate for `filter_by`, `reject_by`, and `any_of_by`.

   `keys` holds the helper arguments that follow the container (each is a
   string). The first is a dot-path key; the rest, if any, are values.
   Two forms are supported:

   - One key: the element matches when the value at that key is truthy.
     Used for boolean fields or presence checks.

         {{filter_by members "isListedOnPrimary"}}   keep truthy
         {{any_of_by shadows "doc"}}                  any has a doc?
         {{any_of_by members "doc.brief"}}            dot-path works too

   - A key plus one or more values: the element matches when the value at
     the key equals any of those values. This filters by a reflected
     enum/string field directly, instead of precomputed booleans:

         {{filter_by symbol.bases "access" "public"}}          == "public"
         {{filter_by members "extraction" "regular" "see-below"}} in {..}

   Non-string keys are ignored (the element does not match), so a
   mis-typed argument fails closed rather than throwing. `el` that is not
   an object simply has no value at the key and does not match.
*/
inline
bool
matchesFilterKeys(dom::Value const& el, std::vector<dom::Value> const& keys)
{
    if (keys.empty() || !keys.front().isString())
    {
        return false;
    }
    dom::Value const actual = el.lookup(keys.front().getString());
    if (keys.size() == 1)
    {
        return actual.isTruthy();
    }
    for (std::size_t i = 1; i < keys.size(); ++i)
    {
        if (actual == keys[i])
        {
            return true;
        }
    }
    return false;
}

auto size_fn = dom::makeInvocable([](
        dom::Value const& val)
    {
        return val.size();
    });

auto keys_fn = dom::makeInvocable([](
        dom::Value container) -> dom::Value
    {
        if (!container.isObject())
        {
            return container;
        }
        auto const& obj = container.getObject();
        dom::Array res;
        obj.visit([&res](auto const& key, auto const&)
        {
            res.emplace_back(key);
        });
        return res;
    });

auto values_fn = dom::makeInvocable([](
        dom::Value container) -> dom::Value
    {
        if (!container.isObject())
        {
            return container;
        }
        auto const& obj = container.getObject();
        dom::Array res;
        obj.visit([&res](auto const&, auto const& value)
        {
            res.emplace_back(value);
        });
        return res;
    });

auto del_fn = dom::makeInvocable([](
        dom::Value range, dom::Value const& item) -> dom::Value {
        if (range.isArray())
        {
            auto const& arr = range.getArray();
            auto const& val = item;
            dom::Array res;
            auto const n = static_cast<std::int64_t>(arr.size());
            for (std::int64_t i = 0; i < n; i++)
            {
                if (arr.get(i) != val)
                {
                    res.emplace_back(arr.at(i));
                }
            }
            return res;
        }
        else if (range.isObject())
        {
            auto const& obj = range.getObject();
            auto const& key = item.getString();
            dom::Object res;
            obj.visit([&res, &key](auto const& k, auto const& v)
            {
                if (k != key)
                {
                    res.set(k, v);
                }
            });
            return res;
        }
        else
        {
            return range;
        }
    });

auto has_fn = dom::makeInvocable([](
        dom::Value const& ctx, dom::Value const& prop)
    {
        if (ctx.isObject())
        {
            dom::Value const& objV = ctx;
            auto const& obj = objV.getObject();
            auto const& key = prop.getString();
            return obj.exists(key);
        }
        else if (ctx.isArray())
        {
            dom::Value const& arrV = ctx;
            auto const& arr = arrV.getArray();
            auto const& value = prop;
            std::size_t const n = arr.size();
            for (std::size_t i = 0; i < n; ++i)
            {
                if (arr.get(i) == value)
                {
                    return true;
                }
            }
        }
        return false;
    });

auto has_any_fn = dom::makeInvocable([](
        dom::Value const& container, dom::Value const& item)
    {
        if (container.isObject())
        {
            auto const& obj = container.getObject();
            dom::Value const& keysV = item;
            auto const& keys = keysV.getArray();
            auto const n = static_cast<std::int64_t>(keys.size());
            for (std::int64_t i = 0; i < n; ++i)
            {
                dom::Value k = keys.get(i);
                if (obj.exists(k.getString()))
                {
                    return true;
                }
            }
            return false;
        }
        else if (container.isArray())
        {
            auto const& arr = container.getArray();
            auto const& values = item.getArray();
            auto n = static_cast<std::int64_t>(values.size());
            for (std::int64_t i = 0; i < n; ++i)
            {
                std::size_t const n2 = arr.size();
                for (std::size_t j = 0; j < n2; ++j)
                {
                    dom::Value a = arr.get(j);
                    dom::Value b = values.get(i);
                    if (a == b)
                    {
                        return true;
                    }
                }
            }
            return false;
        }
        else
        {
            return false;
        }
    });

auto get_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments)
    {
        std::size_t const n = arguments.size();
        dom::Value container = arguments.at(0);
        dom::Value field = arguments.at(1);
        dom::Value default_value = nullptr;
        if (n > 3)
        {
            default_value = arguments.at(2);
        }

        if (container.isArray())
        {
            auto const& arr = container.getArray();
            auto index = field.getInteger();
            if (index < 0)
            {
                index = detail::normalize_index(index, static_cast<std::int64_t>(arr.size()));
            }
            if (index >= static_cast<std::int64_t>(arr.size()))
            {
                return default_value;
            }
            return arr.at(index);
        }
        else if (container.isObject())
        {
            auto const& obj = container.getObject();
            auto const& key = field.getString();
            if (obj.exists(key))
            {
                return obj.get(key);
            }
            return default_value;
        }
        else
        {
            return default_value;
        }
    });

auto items_fn = dom::makeInvocable([](
        dom::Value items) -> dom::Value
    {
        if (items.isObject())
        {
            auto const& obj = items.getObject();
            dom::Array res;
            obj.visit([&res](auto const& key, auto const& value)
            {
                dom::Array item;
                item.emplace_back(key);
                item.emplace_back(value);
                res.emplace_back(item);
            });
            return res;
        }
        else
        {
            return items;
        }
    });

auto first_fn = dom::makeInvocable([](
        dom::Value range) -> dom::Value {
        if (range.isArray())
        {
            auto const& arr = range.getArray();
            if (arr.empty())
            {
                return nullptr;
            }
            return arr.at(0);
        }
        else if (range.isObject())
        {
            auto const& obj = range.getObject();
            if (obj.empty())
            {
                return dom::Kind::Undefined;
            }
            dom::Value res;
            obj.visit([&](dom::String const&, dom::Value const& value) -> bool
            {
                res = value;
                return false;
            });
            return res;
        }
        else
        {
            return range;
        }
    });

auto last_fn = dom::makeInvocable([](
        dom::Value range) -> dom::Value
    {
        if (range.isArray())
        {
            auto const& arr = range.getArray();
            if (arr.empty())
            {
                return {};
            }
            return arr.back();
        }
        else if (range.isObject())
        {
            auto const& obj = range.getObject();
            if (obj.empty())
            {
                return dom::Kind::Undefined;
            }
            dom::Value res;
            obj.visit([&](dom::String const&, dom::Value const& value)
            {
                res = value;
            });
            return res;
        }
        else
        {
            return range;
        }
    });

auto reverse_fn = dom::makeInvocable([](
        dom::Value container) -> dom::Value
    {
        if (container.isArray())
        {
            auto const& arr = container.getArray();
            dom::Array res;
            for (std::size_t i = arr.size(); i > 0; --i)
            {
                res.emplace_back(arr.at(i - 1));
            }
            return res;
        }
        else if (container.isObject())
        {
            auto const& obj = container.getObject();
            dom::Array res;
            obj.visit([&res](dom::String const& key, dom::Value const& value)
            {
                dom::Array item;
                item.emplace_back(key);
                item.emplace_back(value);
                res.emplace_back(item);
            });
            dom::Array reversed;
            for (std::size_t i = res.size(); i > 0; --i) {
                reversed.emplace_back(res.at(i - 1));
            }
            return reversed;
        }
        else
        {
            return container;
        }
    });

auto update_fn = dom::makeInvocable([](
        dom::Value container, dom::Value const& items) -> dom::Value {
        if (container.isObject())
        {
            auto const& obj = container.getObject();
            auto const& other = items.getObject();
            dom::Object res = createFrame(obj);
            other.visit([&res](dom::String const& k, dom::Value const& v)
            {
                res.set(k, v);
            });
            return res;
        }
        else if (container.isArray())
        {
            auto const& arr = container.getArray();
            auto const& other = items.getArray();
            dom::Array res;
            std::size_t const n = arr.size();
            for (std::size_t i = 0; i < n; ++i)
            {
                res.emplace_back(arr.at(i));
            }
            std::size_t const n2 = other.size();
            for (std::size_t i = 0; i < n2; ++i)
            {
                bool arr_contains = false;
                std::size_t const n3 = res.size();
                for (std::size_t j = 0; j < n3; ++j)
                {
                    if (res.at(j) == other.at(i))
                    {
                        arr_contains = true;
                        break;
                    }
                }
                if (!arr_contains)
                {
                    res.emplace_back(other.at(i));
                }
            }
            return res;
        }
        else
        {
            return container;
        }
    });

auto sort_fn = dom::makeInvocable([](
        dom::Value container) -> dom::Value
    {
        if (container.isArray())
        {
            auto const& arr = container.getArray();
            std::vector<dom::Value> res;
            std::size_t const n = arr.size();
            for (std::size_t i = 0; i < n; ++i)
            {
                res.emplace_back(arr.at(i));
            }
            std::stable_sort(res.begin(), res.end(), [](auto const& a, auto const& b) {
                return a < b;
            });
            dom::Array res2;
            for (const auto & re : res) {
                res2.emplace_back(re);
            }
            return res2;
        }
        else
        {
            return container;
        }
    });

auto sort_by_fn = dom::makeInvocable([](
        dom::Value container, dom::Value const& keyV) -> dom::Value
    {
        // Given an array of objects, sort these objects by their value at
        // a given key
        if (container.isArray())
        {
            auto const& arr = container.getArray();
            auto const& key = keyV.getString();
            std::vector<dom::Value> res;
            std::size_t const n = arr.size();
            for (std::size_t i = 0; i < n; ++i) {
                res.emplace_back(arr.at(i));
            }
            std::ranges::stable_sort(res, [&key](dom::Value const& a, dom::Value const& b)
            {
                // If the value is not an object, then we can't sort it
                // by a key, so we just sort it by the value itself
                if (!a.isObject() || !b.isObject())
                {
                    if (a.isObject())
                    {
                        return true;
                    }
                    if (b.isObject())
                    {
                        return false;
                    }
                    return a < b;
                }
                // If the value is an object but the key doesn't exist,
                // then we can't sort it by a key, so we just sort it by
                // whichever has the key
                const bool ak = a.getObject().exists(key);
                const bool bk = b.getObject().exists(key);
                if (!ak)
                {
                    return bk;
                }
                if (!bk)
                {
                    return false;
                }
                // If the value is an object and the key exists, then we
                // sort it by the value at the key
                return a.getObject().get(key) < b.getObject().get(key);
            });
            return dom::Array(res);
        }
        else
        {
            // If the value is not an array, then we can't sort it
            return container;
        }
    });

auto filter_by_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments) -> dom::Value
    {
        dom::Value container = arguments.at(0);
        std::vector<dom::Value> keys;
        for (std::size_t i = 1; i < arguments.size() - 1; ++i)
        {
            dom::Value key = arguments.at(i);
            keys.push_back(key);
        }

        // Given an array of objects, filter these objects by values at
        // a given key. If the value at that key returns true, then the
        // object is included in the result.
        if (container.isArray())
        {
            auto const& arr = container.getArray();

            std::vector<dom::Value> res;
            std::size_t const n = arr.size();
            for (std::size_t i = 0; i < n; ++i) {
                dom::Value el = arr.at(i);

                // If the value is not an object, then we can't filter it
                // by a key
                if (!el.isObject())
                {
                    continue;
                }

                // Keep the element when it matches the key (truthy) or
                // the key/value predicate. See `matchesFilterKeys`.
                if (!matchesFilterKeys(el, keys))
                {
                    continue;
                }

                res.emplace_back(el);
            }
            return dom::Array(res);
        }

        // If the value is not an array, then we can't filter it
        return container;
    });

auto reject_by_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments) -> dom::Value
    {
        dom::Value container = arguments.at(0);
        std::vector<dom::Value> keys;
        for (std::size_t i = 1; i < arguments.size() - 1; ++i)
        {
            dom::Value key = arguments.at(i);
            keys.push_back(key);
        }

        // Given an array of objects, reject (exclude) those whose
        // value at any of the given keys is truthy. Inverse of
        // `filter_by`: non-object elements are kept, since they
        // have no keys to test.
        if (container.isArray())
        {
            dom::Array const& arr = container.getArray();

            std::vector<dom::Value> res;
            std::size_t const n = arr.size();
            for (std::size_t i = 0; i < n; ++i) {
                dom::Value el = arr.at(i);

                if (!el.isObject())
                {
                    res.emplace_back(el);
                    continue;
                }

                // Drop the element when it matches the key (truthy) or
                // the key/value predicate. See `matchesFilterKeys`.
                if (matchesFilterKeys(el, keys))
                {
                    continue;
                }

                res.emplace_back(el);
            }
            return dom::Array(res);
        }

        // If the value is not an array, then we can't reject from it
        return container;
    });

auto any_of_by_fn = dom::makeVariadicInvocable([](
        dom::Array const& arguments) -> dom::Value
    {
        dom::Value container = arguments.at(0);
        std::vector<dom::Value> keys;
        for (std::size_t i = 1; i < arguments.size() - 1; ++i)
        {
            dom::Value key = arguments.at(i);
            keys.push_back(key);
        }

        // Given an array of objects, any_of these objects by values at
        // a given key. If the value at that key returns true, then the
        // object is included in the result.
        if (container.isArray())
        {
            auto const& arr = container.getArray();

            std::vector<dom::Value> res;
            std::size_t const n = arr.size();
            for (std::size_t i = 0; i < n; ++i) {
                dom::Value el = arr.at(i);

                // If the value is not an object, then we can't any_of it
                // by a key
                if (!el.isObject())
                {
                    continue;
                }

                // Return true when the element matches the key (truthy)
                // or the key/value predicate. See `matchesFilterKeys`.
                if (!matchesFilterKeys(el, keys))
                {
                    continue;
                }

                return true;
            }
            return false;
        }

        // If the value is not an array, then we can't any_of it
        return container;
    });

auto fill_fn = dom::makeInvocable([](
        dom::Value container,
        dom::Value const& fill_value,
        dom::Value const& startV,
        dom::Value const& stopV) -> dom::Value {
        if (container.isArray())
        {
            auto const& arr = container.getArray();
            std::int64_t start = 0;
            if (startV.isInteger())
            {
                start = startV.getInteger();
            }
            auto const n = static_cast<std::int64_t>(arr.size());
            auto stop = n;
            if (stopV.isInteger())
            {
                stop = stopV.getInteger();
            }
            start = detail::normalize_index(start, n);
            stop = detail::normalize_index(stop, n);
            dom::Array res;
            for (std::int64_t i = 0; i < n; ++i)
            {
                if (i >= start && i < stop)
                {
                    res.emplace_back(fill_value);
                }
                else
                {
                    res.emplace_back(arr.at(i));
                }
            }
            return res;
        }
        else
        {
            // If the value is not an array, then we can't fill it
            return container;
        }
    });

auto flatten_fn = dom::makeInvocable([](dom::Value const& collection, dom::Value const& key) -> dom::Value
    {
        dom::Array result;

        if (!collection.isArray())
        {
            return result;
        }

        std::string const keyPath(key.getString());
        auto const keys = detail::parseKeyPath(keyPath);

        auto const& arr = collection.getArray();
        for (auto const& item : arr)
        {
            dom::Value const innerCollection = detail::getNestedValue(item, keys);
            if (innerCollection.isArray())
            {
                auto const& innerArray = innerCollection.getArray();
                for (auto const& innerItem : innerArray)
                {
                    result.emplace_back(innerItem);
                }
            }
        }

        return result;
    });

auto flattenUnique_fn = dom::makeInvocable([](dom::Value const& collection, dom::Value const& key, dom::Value const& uniqueKey) -> dom::Value
    {
        dom::Array result;
        std::unordered_set<std::string> seen;

        if (!collection.isArray())
        {
            return result;
        }

        if (key.empty() || uniqueKey.empty())
        {
            return result;
        }

        if (!key.isString() || !uniqueKey.isString())
        {
            return result;
        }

        std::string const keyPath(key.getString());
        auto const keys = detail::parseKeyPath(keyPath);
        std::string const uniqueKeyPath(uniqueKey.getString());
        auto const uniqueKeys = detail::parseKeyPath(uniqueKeyPath);

        auto const& arr = collection.getArray();
        for (auto const& item : arr)
        {
            dom::Value const innerCollection = detail::getNestedValue(item, keys);
            if (innerCollection.isArray())
            {
                auto const& innerArray = innerCollection.getArray();
                for (auto const& innerItem : innerArray)
                {
                    dom::Value const uniqueValue = detail::getNestedValue(innerItem, uniqueKeys);
                    if (uniqueValue.isString())
                    {
                        std::string uniqueStr(uniqueValue.getString());
                        if (seen.find(uniqueStr) == seen.end())
                        {
                            seen.insert(uniqueStr);
                            result.emplace_back(innerItem);
                        }
                    }
                }
            }
        }

        return result;
    });

// Access & inspection: sizes, key/value access, and endpoints.
void
registerContainerAccessHelpers(Handlebars& hbs)
{
    hbs.registerHelper("size", size_fn);
    hbs.registerHelper("len", size_fn);
    hbs.registerHelper("count", dom::makeVariadicInvocable(detail::count_fn));
    hbs.registerHelper("keys", keys_fn);
    hbs.registerHelper("list", keys_fn);
    hbs.registerHelper("iter", keys_fn);
    hbs.registerHelper("values", values_fn);
    hbs.registerHelper("items", items_fn);
    hbs.registerHelper("entries", items_fn);
    hbs.registerHelper("get", get_fn);
    hbs.registerHelper("get_or", get_fn);
    hbs.registerHelper("at", dom::makeInvocable(detail::at_fn));
    hbs.registerHelper("first", first_fn);
    hbs.registerHelper("head", first_fn);
    hbs.registerHelper("front", first_fn);
    hbs.registerHelper("last", last_fn);
    hbs.registerHelper("tail", last_fn);
    hbs.registerHelper("back", last_fn);
}

// Search & membership: locate elements or test for their presence.
void
registerContainerSearchHelpers(Handlebars& hbs)
{
    hbs.registerHelper("find", dom::makeVariadicInvocable(detail::find_index_fn));
    hbs.registerHelper("index_of", dom::makeVariadicInvocable(detail::find_index_fn));
    hbs.registerHelper("has", has_fn);
    hbs.registerHelper("exist", has_fn);
    hbs.registerHelper("contains", has_fn);
    hbs.registerHelper("has_any", has_any_fn);
    hbs.registerHelper("exist_any", has_any_fn);
    hbs.registerHelper("contains_any", has_any_fn);
    hbs.registerHelper("any_of_by", any_of_by_fn);
}

// Ordering & selection: reorder a range or select a subset of it.
void
registerContainerOrderingHelpers(Handlebars& hbs)
{
    hbs.registerHelper("reverse", reverse_fn);
    hbs.registerHelper("reversed", reverse_fn);
    hbs.registerHelper("sort", sort_fn);
    hbs.registerHelper("sort_by", sort_by_fn);
    hbs.registerHelper("filter_by", filter_by_fn);
    hbs.registerHelper("reject_by", reject_by_fn);
}

// Mutation: remove, combine, or overwrite entries.
void
registerContainerMutationHelpers(Handlebars& hbs)
{
    hbs.registerHelper("del", del_fn);
    hbs.registerHelper("delete", del_fn);
    hbs.registerHelper("update", update_fn);
    hbs.registerHelper("merge", update_fn);
    hbs.registerHelper("fill", fill_fn);
    hbs.registerHelper("replace", dom::makeVariadicInvocable(detail::replace_fn));
}

// Transformation: derive a new container from an existing one.
void
registerContainerTransformHelpers(Handlebars& hbs)
{
    hbs.registerHelper("chunk", dom::makeInvocable([](
        dom::Value range, dom::Value const& sizeV) -> dom::Value
    {
        std::int64_t chunkSize = sizeV.getInteger();
        if (range.isArray())
        {
            auto const& arr = range.getArray();
            dom::Array res;
            std::int64_t i = 0;
            auto const n = static_cast<std::int64_t>(arr.size());
            while (i < n)
            {
                dom::Array chunk;
                for (std::int64_t j = 0; j < chunkSize && i < n; ++j)
                {
                    chunk.emplace_back(arr.at(i));
                    ++i;
                }
                res.emplace_back(chunk);
            }
            return res;
        }
        else if (range.isString())
        {
            auto const& str = range.getString();
            dom::Array res;
            std::int64_t i = 0;
            auto const n = static_cast<std::int64_t>(str.size());
            while (i < n)
            {
                std::string chunk;
                for (std::int64_t j = 0; j < chunkSize && i < n; ++j)
                {
                    chunk += str.get()[i];
                    ++i;
                }
                res.emplace_back(chunk);
            }
            return res;
        }
        else if (range.isObject())
        {
            auto const& obj = range.getObject();
            dom::Array res;
            dom::Object chunk;
            obj.visit([&](dom::String const& k, dom::Value const& v)
            {
                chunk.set(k, v);
                if (std::cmp_greater_equal(chunk.size(), chunkSize))
                {
                    res.emplace_back(chunk);
                    chunk = dom::Object();
                }
            });
            if (!chunk.empty())
            {
                res.emplace_back(chunk);
            }
            return res;
        }
        else
        {
            return range;
        }
    }));
    hbs.registerHelper("group_by", dom::makeInvocable([](
        dom::Value range, dom::Value const& keyV) -> dom::Value
    {
        // Given an array of objects, group these objects by a key in each them
        // The result is an object with keys being the values of the key in each
        // object, and the values being arrays of objects with that key value
        if (!range.isArray())
        {
            return range;
        }
        dom::Array const& array = range.getArray();
        std::string key(keyV.getString());
        dom::Array::size_type n = array.size();
        std::vector<std::uint8_t> copied(n, 0x00);
        dom::Object res;
        for (std::size_t i = 0; i < n; ++i)
        {
            if (copied[i] != 0x00 || !array.get(i).isObject() || !array.get(i).getObject().exists(key))
            {
                // object already copied or doesn't have the key
                copied[i] = 0x01;
                continue;
            }
            copied[i] = 0x01;

            // Create a group for this key value
            std::string group_name(toString(array.get(i).get(key)));
            dom::Array group;
            group.emplace_back(array.get(i));

            // Copy any other equivalent keys to the same group
            for (std::size_t j = i; j < n; ++j)
            {
                if (copied[j])
                {
                    continue;
                }
                if (array.get(j).get(key).getString() == array.get(i).get(key).getString())
                {
                    group.emplace_back(array.get(j));
                    copied[j] = 0x01;
                }
            }
            res.set(group_name, group);
        }
        return res;
    }));
    hbs.registerHelper("pluck", dom::makeInvocable([](
        dom::Value rangeV, dom::Value const& keyV) -> dom::Value
    {
        if (!rangeV.isArray())
        {
            return rangeV;
        }
        // Given an array of objects, take the value at `key` from each
        // object. `key` may be a dotted path (e.g. "type.name.id"), which
        // is walked one segment at a time. An element whose path does not
        // fully resolve yields `undefined`.
        std::string_view const key = keyV.getString();
        auto resolve = [&](dom::Value v) -> dom::Value
        {
            std::size_t pos = 0;
            while (pos < key.size())
            {
                std::size_t const dot = key.find('.', pos);
                std::string const seg(key.substr(pos,
                    dot == std::string_view::npos
                        ? std::string_view::npos : dot - pos));
                pos = dot == std::string_view::npos ? key.size() : dot + 1;
                if (!v.isObject() || !v.getObject().exists(seg))
                {
                    return {dom::Kind::Undefined};
                }
                v = v.getObject().get(seg);
            }
            return v;
        };
        dom::Array const& range = rangeV.getArray();
        dom::Array res;
        auto n = static_cast<std::int64_t>(range.size());
        for (std::int64_t i = 0; i < n; ++i)
        {
            res.emplace_back(resolve(range.get(i)));
        }
        return res;
    }));

    // Combine parallel columns into an array of objects (the row-wise
    // dual of `pluck`). Given a keys array and one column per key, it
    // returns one object per index i, mapping each key to the i-th value
    // of its column. This is the "zip into records" operation (cf. Python
    // `dict(zip(keys, row))`, lodash `zipObject` applied per row): it lets
    // a caller assemble table rows from separately-derived columns while a
    // generic table just reads plain fields. The row count is the longest
    // column; a shorter column yields `undefined` for its missing rows.
    //
    //   zip_objects (arr "name" "brief") names briefs
    //     -> [ { name: names[0], brief: briefs[0] }, ... ]
    hbs.registerHelper("zip_objects", dom::makeVariadicInvocable([](
        dom::Array const& arguments) -> dom::Value
    {
        // The last argument is the helper options object. Copy the keys
        // and column values into owning locals: `arguments.at(i)` yields a
        // temporary, so binding a reference to its array/string would
        // dangle.
        std::size_t const nArgs = arguments.size() - 1;
        if (nArgs < 1 || !arguments.at(0).isArray())
        {
            return dom::Array{};
        }
        dom::Value const keysV = arguments.at(0);
        dom::Array const& keys = keysV.getArray();
        std::vector<dom::Value> cols;
        for (std::size_t c = 1; c < nArgs; ++c)
        {
            cols.push_back(arguments.at(c));
        }
        std::size_t rows = 0;
        for (dom::Value const& colV : cols)
        {
            if (colV.isArray())
            {
                rows = (std::max)(rows, colV.getArray().size());
            }
        }
        dom::Array res;
        for (std::size_t i = 0; i < rows; ++i)
        {
            dom::Object row;
            for (std::size_t k = 0; k < keys.size(); ++k)
            {
                dom::Value v{dom::Kind::Undefined};
                if (k < cols.size() && cols[k].isArray())
                {
                    dom::Array const& col = cols[k].getArray();
                    if (i < col.size())
                    {
                        v = col.at(i);
                    }
                }
                dom::Value const keyV = keys.at(k);
                row.set(std::string(keyV.getString()), v);
            }
            res.emplace_back(std::move(row));
        }
        return res;
    }));
    hbs.registerHelper("unique", dom::makeInvocable([](
        dom::Value rangeV) -> dom::Value
    {
        if (!rangeV.isArray())
        {
            return rangeV;
        }
        // remove duplicates from an array
        dom::Array const& range = rangeV.getArray();
        std::vector<dom::Value> res;
        for (auto const& el: range)
        {
            res.push_back(el);
        }
        std::ranges::sort(res, [](dom::Value const& a, dom::Value const& b)
        {
            return a < b;
        });
        auto [first, last] = std::ranges::unique(res, [](
            dom::Value const& a, dom::Value const& b)
        {
            return a == b;
        });
        res.erase(first, res.end());
        dom::Array res2;
        for (auto const& v : res) {
            res2.emplace_back(v);
        }
        return res2;
    }));
    hbs.registerHelper("concat", dom::makeVariadicInvocable(detail::concat_fn));
    hbs.registerHelper("flatten", flatten_fn);
    hbs.registerHelper("flattenUnique", flattenUnique_fn);

    // `transform array fn` -- apply the function `fn` to each element of
    // `array` and return the resulting array. `fn` is any callable value,
    // including one reached through the context (e.g.
    // `transform ids @root.mrdocs.corpus.get` to resolve a list of ids to
    // symbols). A non-array first argument is returned unchanged.
    hbs.registerHelper("transform", dom::makeVariadicInvocable(
        [](dom::Array const& args) -> Expected<dom::Value, dom::Error>
        {
            if (args.size() < 2)
            {
                return Unexpected(dom::Error(
                    "transform: expected (array, function)"));
            }
            dom::Value const rangeV = args.get(0);
            dom::Value const fnV = args.get(1);
            if (!rangeV.isArray())
            {
                return rangeV;
            }
            if (!fnV.isFunction())
            {
                return Unexpected(dom::Error(
                    "transform: second argument must be a function"));
            }
            auto fn = fnV.getFunction();
            dom::Array const& range = rangeV.getArray();
            dom::Array res;
            for (std::size_t i = 0; i < range.size(); ++i)
            {
                dom::Array callArgs;
                callArgs.emplace_back(range.get(i));
                Expected<dom::Value, dom::Error> r = fn.call(callArgs);
                if (!r)
                {
                    return Unexpected(r.error());
                }
                res.emplace_back(*r);
            }
            return dom::Value(res);
        }));
}

} // namespace container_helpers_detail

void
registerContainerHelpers(Handlebars& hbs)
{
    // Registration is split across several functions both to group the
    // helpers by what they do and to keep each function small: this
    // translation unit defines many helpers as sizeable lambdas, and
    // folding every `registerHelper` call into one body inflates compile
    // time and memory and can hit compiler function-complexity limits
    // (notably MSVC). The groups are by semantic category, so a new
    // helper goes with the ones whose job it shares.
    container_helpers_detail::registerContainerAccessHelpers(hbs);
    container_helpers_detail::registerContainerSearchHelpers(hbs);
    container_helpers_detail::registerContainerOrderingHelpers(hbs);
    container_helpers_detail::registerContainerMutationHelpers(hbs);
    container_helpers_detail::registerContainerTransformHelpers(hbs);
}

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs
