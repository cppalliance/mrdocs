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

                // If the value is an object but the key doesn't exist,
                // then we can't filter it by a key, so we just filter it by
                // whichever has the key
                auto matchIt = std::ranges::find_if(keys, [&](dom::Value const& key)
                {
                    return
                        el.getObject().exists(key.getString()) &&
                        el.getObject().get(key.getString()).isTruthy();
                });
                if (bool const matchAny = matchIt != keys.end();
                    !matchAny)
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

                auto const matchIt = std::ranges::find_if(
                    keys,
                    [&](dom::Value const& key)
                    {
                        return
                            el.getObject().exists(key.getString()) &&
                            el.getObject().get(key.getString()).isTruthy();
                    });
                if (matchIt != keys.end())
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

                // If the value is an object but the key doesn't exist,
                // then we can't any_of it by a key, so we just any_of it by
                // whichever has the key
                auto matchIt = std::ranges::find_if(keys, [&](dom::Value const& key)
                {
                    return el.lookup(key.getString()).isTruthy();
                });
                if (bool const matchAny = matchIt != keys.end();
                    !matchAny)
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

void
registerContainerHelpers_1(Handlebars& hbs)
{
    hbs.registerHelper("size", size_fn);
    hbs.registerHelper("len", size_fn);
    hbs.registerHelper("keys", keys_fn);
    hbs.registerHelper("list", keys_fn);
    hbs.registerHelper("iter", keys_fn);
    hbs.registerHelper("values", values_fn);
    hbs.registerHelper("del", del_fn);
    hbs.registerHelper("delete", del_fn);
    hbs.registerHelper("find", dom::makeVariadicInvocable(detail::find_index_fn));
    hbs.registerHelper("index_of", dom::makeVariadicInvocable(detail::find_index_fn));
    hbs.registerHelper("has", has_fn);
    hbs.registerHelper("exist", has_fn);
}

void
registerContainerHelpers_2(Handlebars& hbs)
{
    hbs.registerHelper("contains", has_fn);
    hbs.registerHelper("has_any", has_any_fn);
    hbs.registerHelper("exist_any", has_any_fn);
    hbs.registerHelper("contains_any", has_any_fn);
    hbs.registerHelper("get", get_fn);
    hbs.registerHelper("get_or", get_fn);
    hbs.registerHelper("items", items_fn);
    hbs.registerHelper("entries", items_fn);
    hbs.registerHelper("first", first_fn);
    hbs.registerHelper("head", first_fn);
    hbs.registerHelper("front", first_fn);
    hbs.registerHelper("last", last_fn);
}

void
registerContainerHelpers_3(Handlebars& hbs)
{
    hbs.registerHelper("tail", last_fn);
    hbs.registerHelper("back", last_fn);
    hbs.registerHelper("reverse", reverse_fn);
    hbs.registerHelper("reversed", reverse_fn);
    hbs.registerHelper("update", update_fn);
    hbs.registerHelper("merge", update_fn);
    hbs.registerHelper("sort", sort_fn);
    hbs.registerHelper("sort_by", sort_by_fn);
    hbs.registerHelper("filter_by", filter_by_fn);
    hbs.registerHelper("reject_by", reject_by_fn);
    hbs.registerHelper("any_of_by", any_of_by_fn);
    hbs.registerHelper("at", dom::makeInvocable(detail::at_fn));
}

void
registerContainerHelpers_4(Handlebars& hbs)
{
    hbs.registerHelper("fill", fill_fn);
    hbs.registerHelper("count", dom::makeVariadicInvocable(detail::count_fn));
    hbs.registerHelper("replace", dom::makeVariadicInvocable(detail::replace_fn));
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
        // Given an array of objects, take the value of a key from each object
        dom::Array const& range = rangeV.getArray();
        std::string key(keyV.getString());
        dom::Array res;
        auto n = static_cast<std::int64_t>(range.size());
        for (std::int64_t i = 0; i < n; ++i)
        {
            if (range.get(i).isObject() && range.get(i).getObject().exists(key))
            {
                res.emplace_back(range.get(i).getObject().get(key));
            }
        }
        return res;
    }));}

void
registerContainerHelpers_4b(Handlebars& hbs)
{
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
}

} // namespace container_helpers_detail

void
registerContainerHelpers(Handlebars& hbs)
{
    container_helpers_detail::registerContainerHelpers_1(hbs);
    container_helpers_detail::registerContainerHelpers_2(hbs);
    container_helpers_detail::registerContainerHelpers_3(hbs);
    container_helpers_detail::registerContainerHelpers_4(hbs);
    container_helpers_detail::registerContainerHelpers_4b(hbs);
}

} // namespace helpers
} // namespace handlebars
} // namespace mrdocs
