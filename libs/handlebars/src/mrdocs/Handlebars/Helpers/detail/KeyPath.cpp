//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2023 Alan de Freitas (alandefreitas@gmail.com)
//
// Official repository: https://github.com/cppalliance/mrdocs
//


#include <mrdocs/Handlebars/Helpers/detail/KeyPath.hpp>
#include <mrdocs/Handlebars.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace mrdocs {
namespace handlebars {
namespace detail {

std::vector<std::string> 
parseKeyPath(std::string const& keyPath)
{
    std::vector<std::string> keys;
    std::istringstream iss(keyPath);
    std::string token;

    while (std::getline(iss, token, '.'))
    {
        keys.push_back(token);
    }

    return keys;
}

dom::Value
getNestedValue(dom::Value const& obj, std::vector<std::string> const& keys)
{
    dom::Value current = obj;
    for (auto const& key : keys)
    {
        if (current.isObject() && current.getObject().exists(key))
        {
            current = current.getObject().get(key);
        }
        else
        {
            return {};
        }
    }
    return current;
}


} // namespace detail
} // namespace handlebars
} // namespace mrdocs
