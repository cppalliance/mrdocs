#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

import sys
import json
import os


def to_camel_case(kebab_str):
    if ' ' in kebab_str:
        parts = kebab_str.split(' ')
        return to_camel_case('-'.join(parts))

    if '.' in kebab_str:
        parts = kebab_str.split('.')
        return '.'.join([to_camel_case(part) for part in parts])

    kebab_str = kebab_str[0].lower() + kebab_str[1:]
    components = kebab_str.split('-')
    return components[0] + ''.join(x.title() for x in components[1:])


def to_pascal_case(kebab_str):
    separator_chars = [' ', '<', '>', ',']
    for separator_char in separator_chars:
        if separator_char in kebab_str:
            parts = kebab_str.split(separator_char)
            return to_pascal_case('-'.join(parts))

    if '.' in kebab_str:
        parts = kebab_str.split('.')
        return '.'.join([to_pascal_case(part) for part in parts])

    components = kebab_str.split('-')
    return ''.join(x.title() for x in components)


def get_flat_suboptions(option_name, options):
    # Get flat suboptions from an array of options where each option may have suboptions
    flat_suboptions = []
    for option in options:
        has_suboptions = 'options' in option
        if has_suboptions:
            new_flat_suboptions = get_flat_suboptions(option['name'], option['options'])
            for flat_option in new_flat_suboptions:
                flat_option['name'] = f'{option_name}.{flat_option["name"]}'
            flat_suboptions.extend(new_flat_suboptions)
        else:
            flat_option = {
                'name': f'{option_name}.{option["name"]}',
                'brief': option['brief']
            }
            flat_suboptions.append(flat_option)
    return flat_suboptions


def get_valid_enum_categories():
    valid_enum_cats = {
        'log-level': ["trace", "debug", "info", "warn", "error", "fatal"],
        'base-member-inheritance': ["never", "reference", "copy-dependencies", "copy-all"],
        'sort-symbol-by': ["name", "location"]
    }
    return valid_enum_cats


def get_valid_enum_arrays():
    return get_valid_enum_categories().values()


def are_valid_enum_values(enum_values):
    valid_enum_arrays_as_sets = [set(enum_values) for enum_values in get_valid_enum_arrays()]
    return set(enum_values) in valid_enum_arrays_as_sets


def get_enum_category_name(enum_values):
    for [enum_name, valid_enum_values] in get_valid_enum_categories().items():
        if set(enum_values) == set(valid_enum_values):
            return enum_name
    return None


def get_valid_option_values():
    valid_option_values = [
        'bool',
        'unsigned',
        'int',
        'enum',
        'string',
        'string-list',
        'file-path',
        'dir-path',
        'path',
        'path-glob',
        'symbol-glob',
        'list<string>',
        'list<path>',
        'list<path-glob>',
        'list<symbol-glob>',
        'map<string,string>',
        'map<string,object>'
    ]
    return valid_option_values


def is_valid_option_type(option_type):
    return option_type in get_valid_option_values()


def validate_and_normalize_option(option, flat_options):
    # Validate and normalize the configuration options
    if 'name' not in option:
        raise ValueError('Option must have a name')
    if '.' in option['name']:
        raise ValueError(f'Option name "{option["name"]}" cannot contain a "."')
    if 'category' in option:
        raise ValueError(f'Option "{option["name"]}" cannot have a "category" key in its definition')
    if 'command-line-only' not in option:
        option['command-line-only'] = False
    if 'brief' not in option:
        raise ValueError(f'Option "{option["name"]}" must have a "brief" description')
    if 'details' not in option:
        option['details'] = ''
        print(f'Warning: Option "{option["name"]}" has no "details" key. Defaulting to ""')
    if 'type' not in option:
        option['type'] = 'string'
        print(f'Warning: Option "{option["name"]}" has no "type" key. Defaulting to "string"')

    if not is_valid_option_type(option['type']):
        raise ValueError(
            f'Option "{option["name"]}" has an invalid type {option["type"]}: It should be one of {get_valid_option_values()}')

    if option['type'] == 'enum':
        if 'values' not in option:
            raise ValueError(f'Option "{option["name"]}" is of type enum and must have "values"')
        if not are_valid_enum_values(option['values']):
            raise ValueError(
                f'Option "{option["name"]}" has invalid enum values: It should be one of {get_valid_enum_categories()}')
        if 'default' in option and option['default'] not in option['values']:
            raise ValueError(f'Option "{option["name"]}" has an invalid default value for enum')
    if 'default' not in option:
        if option['type'] == 'bool':
            option['default'] = False
        elif option['type'] in ['unsigned', 'int']:
            option['default'] = 0
        elif option['type'] == 'string':
            option['default'] = ''
        elif option['type'].startswith('list<'):
            option['default'] = []
        elif option['type'] == 'enum':
            option['default'] = option['values'][0]
    if option['type'] in ['path', 'file-path', 'dir-path', 'list<path>']:
        if 'relative-to' not in option:
            option['relative-to'] = '<config-dir>'
            print(f'Warning: Option "{option["name"]}" has no "relative-to" key. Defaulting to "<config-dir>"')
        if 'must-exist' not in option:
            option['must-exist'] = True
            print(f'Warning: Option "{option["name"]}" has no "must-exist" key. Defaulting to "true"')
        if 'should-exist' not in option:
            option['should-exist'] = False
            if not option['must-exist']:
                print(f'Warning: Option "{option["name"]}" has no "should-exist" key. Defaulting to "false"')
        reference_directories = ['<config-dir>', '<cwd>', '<mrdocs-root>']
        for flat_option in flat_options:
            if flat_option['name'] == option['name']:
                continue
            # If option is unique directory option of some form
            if flat_option['type'] in ['path', 'dir-path', 'file-path']:
                reference_directories.append(f'<{flat_option["name"]}>')
                reference_directories.append(f'<{flat_option["name"]}-dir>')
        if option['relative-to'] not in reference_directories:
            raise ValueError(f'Option "{option["name"]}" has an invalid value for "relative-to"')
        default_paths = option['default']
        if not isinstance(default_paths, list):
            default_paths = [default_paths]
        for default_path in default_paths:
            if default_path.startswith('<'):
                option_default_starts_with_reference = any(
                    default_path.startswith(reference_dir) for reference_dir in reference_directories)
                if not option_default_starts_with_reference:
                    raise ValueError(
                        f'Option "{option["name"]}" starts with an invalid reference directory "{default_path}". It should be one of {reference_directories}')
    if 'required' not in option:
        option['required'] = False
    if 'command-line-sink' not in option:
        option['command-line-sink'] = False
    if 'options' in option:
        for suboption in option['options']:
            validate_and_normalize_option(suboption)
    return option

def validate_relative_to_options(flat_options):
    # If an option includes a relative-to key or includes
    # default values that start with a reference directory,
    # we have to check if the relative-to key is valid
    # and if the key comes before the current option in the list
    # because the reference directories are defined in the order
    # they appear in the list of options. This is important to
    # ensure that the reference directories are defined before
    # they are used in the default values of other options.
    # After that, we should allow <key> and <key-dir> to be used
    # in the config normalization process implemented in C++.
    hard_coded_reference_directories = ['config-dir', 'cwd', 'mrdocs-root']
    single_directory_option_types = ['path', 'dir-path', 'file-path', 'path-glob']
    list_directory_option_types = [f'list<{option_type}>' for option_type in single_directory_option_types]
    directory_option_types = single_directory_option_types + list_directory_option_types
    for i in range(len(flat_options)):
        option = flat_options[i]
        if option["type"] not in directory_option_types:
            continue

        option_default_directories = []
        if option['default'] is not None:
            if not isinstance(option['default'], list):
                if option['default'].startswith('<'):
                    option_default_directories.append(option['default'])
            else:
                for default in option['default']:
                    if default.startswith('<'):
                        option_default_directories.append(default)

        option_relative_dependencies = []
        if 'relative-to' in option:
            option_relative_dependencies.append(option['relative-to'])
        for option_default_directory in option_default_directories:
            if option_default_directory.startswith('<'):
                pos = option_default_directory.find('>')
                if pos == -1:
                    raise ValueError(f'Option "{option["name"]}" has an invalid default value')
                option_relative_dependencies.append(option_default_directory[0:pos+1])
        assert(all([directory.startswith('<') and directory.endswith('>') for directory in option_relative_dependencies]))
        option_relative_dependencies = [directory[1:-1] for directory in option_relative_dependencies]

        for option_relative_dependency in option_relative_dependencies:
            found = False
            if option_relative_dependency in hard_coded_reference_directories:
                found = True
            else:
                for j in range(i):
                    other_option = flat_options[j]
                    if other_option["type"] not in single_directory_option_types:
                        continue
                    if other_option['name'] == option_relative_dependency:
                        found = True
                        break
                    if other_option['name'] + '-dir' == option_relative_dependency:
                        found = True
                        break
            if not found:
                raise ValueError(f'Option "{option["name"]}" has an invalid relative reference "<{option_relative_dependency}>"')

def validate_and_normalize_config(config):
    # Validate and normalize the configuration options
    flat_options = flat_config_options(config)
    for category in config:
        if 'category' not in category:
            raise ValueError('Category must have a name')
        if not 'command-line-only':
            category['command-line-only'] = False
        if 'brief' not in category:
            raise ValueError(f'Category {category["category"]} must have a "brief" description')
        if 'details' not in category:
            category['details'] = ''
        if 'options' not in category:
            raise ValueError(f'Category {category["category"]} must have "options"')
        for option in category['options']:
            validate_and_normalize_option(option, flat_options)
    validate_relative_to_options(flat_options)
    return config


def find_options_for_enum(options, enum_values):
    # Recursively iterate the config categories and find enum
    # options that accept the given enum values
    result = []
    for option in options:
        if option['type'] == 'enum' and set(option['values']) == set(enum_values):
            result.append(option)
        elif 'options' in option:
            result.extend(find_options_for_enum(option['options'], enum_values))
    return result


def generate_config_schema_hpp(config):
    # Generate a header file with the configuration information
    header_comment = generate_header_comment()
    contents = header_comment

    header_guard = 'MRDOCS_PUBLIC_SETTINGS_HPP'
    contents += f'#ifndef {header_guard}\n'
    contents += f'#define {header_guard}\n\n'

    headers = [
        '<mrdocs/Support/Filesystem/Glob.hpp>',
        '<mrdocs/Support/String/StringList.hpp>',
        '<mrdocs/Support/Error/Error.hpp>',
        '<mrdocs/Config/ReferenceDirectories.hpp>',
        '<mrdocs/Dom/Object.hpp>',
        '<mrdocs/Support/Reflection/Describe.hpp>',
        '<string>',
        '<string_view>',
        '<span>',
        '<vector>',
        '<optional>',
        '<variant>',
        '<map>',
    ]
    for header in headers:
        contents += f'#include {header}\n'
    contents += '\n'

    contents += 'namespace mrdocs {\n\n'
    contents += 'struct ConfigSchema {\n'

    contents += '    //--------------------------------------------\n'
    contents += '    // Enums\n'
    contents += '    //--------------------------------------------\n\n'

    for [enum_name, enum_values] in get_valid_enum_categories().items():
        options_that_use_it = []
        for category in config:
            options_that_use_it.extend(find_options_for_enum(category['options'], enum_values))
        contents += f'    /** Enum for "{enum_name}" options\n'
        if not options_that_use_it:
            raise ValueError(f'Enum "{enum_name}" is not used by any option')
        contents += f'    \n'
        if len(options_that_use_it) == 1:
            contents += f'        This enumeration value is valid for the `{options_that_use_it[0]["name"]}` option\n\n'
        else:
            contents += f'        These enumeration values are valid for the following options:\n\n'
            for option in options_that_use_it:
                contents += f'        - {option["name"]}\n'
            contents += '\n'
        contents += f'     */\n'
        contents += f'    enum class {to_pascal_case(enum_name)} {{\n'
        for enum_value in enum_values:
            contents += f'        {to_pascal_case(enum_value)},\n'
        contents += '    };\n\n'

        contents += f'    static\n'
        contents += f'    constexpr\n'
        contents += f'    std::string_view\n'
        contents += f'    toString({to_pascal_case(enum_name)} const e) {{\n'
        contents += f'        switch (e) {{\n'
        for enum_value in enum_values:
            contents += f'             case {to_pascal_case(enum_name)}::{to_pascal_case(enum_value)}:\n'
            contents += f'                  return "{enum_value}";\n'
        contents += f'        }}\n'
        contents += f'        return "";\n'
        contents += '    }\n\n'

        contents += f'    static\n'
        contents += f'    constexpr\n'
        contents += f'    bool\n'
        contents += f'    fromString(std::string_view const str, {to_pascal_case(enum_name)}& e) {{\n'
        for enum_value in enum_values:
            contents += f'        if (str == "{enum_value}")\n'
            contents += f'        {{\n'
            contents += f'            e = {to_pascal_case(enum_name)}::{to_pascal_case(enum_value)};\n'
            contents += f'            return true;\n'
            contents += f'        }}\n'
        contents += f'        return false;\n'
        contents += '    }\n\n'

    for category in config:
        category_name = category['category']
        category_brief = category['brief']

        contents += '    //--------------------------------------------\n'
        contents += f'    // {category_name}\n'
        if category_brief:
            contents += f'    // \n'
            contents += f'    // {category_brief}\n'
        contents += '    //--------------------------------------------\n\n'

        for option in category['options']:
            contents += indent(generate_option_declaration(option), 4)
            contents += '\n\n'

    flat_options = flat_config_options(config)
    flat_option_types = set([option['type'] for option in flat_options])
    contents += '    /** Option Type\n'
    contents += '      */\n'
    contents += '    enum class OptionType {\n'
    for option_type in flat_option_types:
        contents += f'        {to_pascal_case(option_type)},\n'
    contents += '    };\n\n'
    flat_cpp_option_types = set([to_cpp_type(option) for option in flat_options])

    contents += '    /** Option validation traits\n'
    contents += '     */\n'
    contents += '    struct OptionProperties {\n'
    contents += f'        OptionType {to_camel_case("type")} = OptionType::String;\n'
    contents += f'        bool {to_camel_case("required")} = false;\n'
    contents += f'        bool {to_camel_case("command-line-sink")} = false;\n'
    contents += f'        bool {to_camel_case("command-line-only")} = false;\n'
    contents += f'        bool {to_camel_case("must-exist")} = true;\n'
    contents += f'        bool {to_camel_case("should-exist")} = false;\n'
    contents += f'        std::optional<int> {to_camel_case("min-value")} = std::nullopt;\n'
    contents += f'        std::optional<int> {to_camel_case("max-value")} = std::nullopt;\n'
    contents += f'        std::optional<std::map<std::string, std::string>> {to_camel_case("filename-mapping")} = std::nullopt;\n'
    contents += f'        std::variant<\n'
    contents += f'            std::monostate'
    for cpp_type in flat_cpp_option_types:
        contents += ',\n'
        contents += f'            {cpp_type}'
    contents += f'> {to_camel_case("default")}Value = std::monostate();\n'
    contents += f'        std::string {to_camel_case("relative-to")} = {{}};\n'
    contents += f'        std::optional<std::string> {to_camel_case("deprecated")} = std::nullopt;\n'
    contents += '    };\n\n'

    contents += '    /** Normalize the configuration values with a visitor\n'
    contents += '        \n'
    contents += '        This function normalizes and validates the configuration values.\n'
    contents += '        \n'
    contents += '        @param dirs The reference directories to resolve paths\n'
    contents += '        @param f The visitor\n'
    contents += '        @return Expected<void> with the error if any\n'
    contents += '      */\n'
    contents += '    template <class F>\n'
    contents += '    Expected<void>\n'
    contents += '    normalize(\n'
    contents += '        ReferenceDirectories const& dirs,\n'
    contents += '        F&& f)\n'
    contents += '    {\n'
    for option in flat_options:
        contents += f'        // {option["brief"]}\n'
        contents += f'        MRDOCS_TRY(std::forward<F>(f)(*this, {escape_as_cpp_string(option["name"])}, {to_camel_case(option["name"])}, dirs, OptionProperties{{\n'
        pad = ' ' * 12
        if 'type' in option:
            contents += f'{pad}.{to_camel_case("type")} = OptionType::{to_pascal_case(option["type"])},\n'
        if 'required' in option:
            contents += f'{pad}.{to_camel_case("required")} = {"true" if option["required"] else "false"},\n'
        if 'command-line-sink' in option and option['command-line-sink']:
            contents += f'{pad}.{to_camel_case("command-line-sink")} = {"true" if option["command-line-sink"] else "false"},\n'
        if 'command-line-only' in option and option['command-line-only']:
            contents += f'{pad}.{to_camel_case("command-line-only")} = {"true" if option["command-line-only"] else "false"},\n'
        if 'must-exist' in option:
            contents += f'{pad}.{to_camel_case("must-exist")} = {"true" if option["must-exist"] else "false"},\n'
        if 'should-exist' in option:
            contents += f'{pad}.{to_camel_case("should-exist")} = {"true" if option["should-exist"] else "false"},\n'
        if 'filename-mapping' in option:
            contents += f'{pad}.{to_camel_case("filename-mapping")} = std::map<std::string, std::string>{{\n'
            for [key, value] in option['filename-mapping'].items():
                contents += f'{pad}    {{ {escape_as_cpp_string(key)}, {escape_as_cpp_string(value)} }},\n'
            contents += f'{pad}}},\n'
        if 'default' in option:
            # Print option and its default
            cpp_default_value = to_cpp_default_value(option, False)
            # print the default in cpp
            if cpp_default_value is not None:
                cpp_type = to_cpp_type(option)
                if cpp_type in ['bool', 'unsigned', 'int', 'enum']:
                    cpp_type = f'static_cast<{cpp_type}>'
                contents += f'{pad}.{to_camel_case("default")}Value = {cpp_type}({cpp_default_value}),\n'
        if 'relative-to' in option:
            contents += f'{pad}.{to_camel_case("relative-to")} = {escape_as_cpp_string(option["relative-to"])},\n'
        if 'deprecated' in option:
            contents += f'{pad}.{to_camel_case("deprecated")} = {escape_as_cpp_string(option["deprecated"])},\n'
        contents += f'        }}));\n'
    contents += '        return {};\n'
    contents += '    }\n\n'

    # Command-line help metadata, so `--help` is printed from the schema.
    contents += '    /** Command-line help metadata for one option. */\n'
    contents += '    struct CommandLineOptionInfo {\n'
    contents += '        std::string_view category;\n'
    contents += '        std::string_view name;\n'
    contents += '        std::string_view brief;\n'
    contents += '        bool takesValue;\n'
    contents += '    };\n\n'

    contents += '    /** Every option with its command-line help metadata.\n\n'
    contents += '        `--help` iterates these and prints them grouped by\n'
    contents += '        category, so the help text is driven by the schema.\n'
    contents += '     */\n'
    contents += '    static\n'
    contents += '    std::span<CommandLineOptionInfo const>\n'
    contents += '    commandLineOptionInfos()\n'
    contents += '    {\n'
    contents += '        static constexpr CommandLineOptionInfo infos[] = {\n'
    for category in config:
        category_name = category['category']
        for option in flat_config_options_impl([category], ''):
            takes_value = 'false' if option['type'] == 'bool' else 'true'
            contents += (
                f'            {{ {escape_as_cpp_string(category_name)}, '
                f'{escape_as_cpp_string(option["name"])}, '
                f'{escape_as_cpp_string(option["brief"])}, {takes_value} }},\n')
    contents += '        };\n'
    contents += '        return infos;\n'
    contents += '    }\n\n'

    # Function to visit every option (name and member reference).
    contents += '    /** Call `f(name, member)` for every option\n'
    contents += '        \n'
    contents += '        @param f The callback invoked once per option\n'
    contents += '     */\n'
    contents += '    template <class F>\n'
    contents += '    void\n'
    contents += '    forEach(F&& f)\n'
    contents += '    {\n'
    for option in flat_options:
        contents += f'        std::forward<F>(f)({escape_as_cpp_string(option["name"])}, {to_camel_case(option["name"])});\n'
    contents += '    }\n\n'

    contents += '    /** Call `f(name, member)` for every option\n'
    contents += '        \n'
    contents += '        @param f The callback invoked once per option\n'
    contents += '     */\n'
    contents += '    template <class F>\n'
    contents += '    void\n'
    contents += '    forEach(F&& f) const\n'
    contents += '    {\n'
    for option in flat_options:
        contents += f'        std::forward<F>(f)({escape_as_cpp_string(option["name"])}, {to_camel_case(option["name"])});\n'
    contents += '    }\n\n'

    contents += '}; // struct ConfigSchema\n\n'

    # Describe the struct so the reflection-to-DOM bridge (describedToDom)
    # can project a ConfigSchema the same way it projects symbols, instead
    # of relying on a hand-materialized dom::Object.
    member_names = ', '.join(
        to_camel_case(option["name"]) for option in flat_options)
    contents += f'MRDOCS_DESCRIBE_STRUCT(ConfigSchema, (), ({member_names}))\n\n'

    # Functions to convert enums to strings
    for [enum_name, enum_values] in get_valid_enum_categories().items():
        enum_class_name = to_pascal_case(enum_name)
        contents += f'/** Convert a ConfigSchema::{enum_class_name}" to a string\n'
        contents += f' */\n'
        contents += f'constexpr\n'
        contents += f'std::string_view\n'
        contents += f'to_string(ConfigSchema::{enum_class_name} e)\n'
        contents += '{\n'
        contents += f'    switch (e)\n'
        contents += '    {\n'
        for enum_value in enum_values:
            contents += f'        case ConfigSchema::{enum_class_name}::{to_pascal_case(enum_value)}:\n'
            contents += f'            return "{enum_value}";\n'
        contents += '    }\n'
        contents += '    return {};\n'
        contents += '}\n\n'

    # Describe each enum so the reflection-based config loader can map a
    # YAML string to its enumerator (via describe::describe_enumerators and
    # toString), instead of a hand-written llvm::yaml ScalarEnumerationTraits.
    for [enum_name, enum_values] in get_valid_enum_categories().items():
        enum_class_name = to_pascal_case(enum_name)
        enumerators = ', '.join(
            to_pascal_case(enum_value) for enum_value in enum_values)
        contents += (
            f'MRDOCS_DESCRIBE_ENUM('
            f'ConfigSchema::{enum_class_name}, {enumerators})\n\n')

    contents += '} // namespace mrdocs\n\n'
    contents += f'#endif // {header_guard}\n'
    return contents


def flat_config_options_impl(config, parent_name):
    flat_options = []
    for category in config:
        for option in category['options']:
            option_copy = option.copy()
            if parent_name:
                option_copy['name'] = f'{parent_name}.{option["name"]}'
            if 'options' not in option:
                # If the option does not have suboptions, it is a final option
                # Update its name to include the parent option's name, if any
                flat_options.append(option_copy)
            else:
                # If the option has suboptions, recursively flatten them
                suboptions = flat_config_options_impl([{'category': '', 'options': option['options']}],
                                                      option_copy['name'])
                for suboption in suboptions:
                    # Update the name of the suboption to include the parent option's name
                    if parent_name:
                        suboption['name'] = f'{suboption["name"]}'
                    flat_options.append(suboption)
    return flat_options


def flat_config_options(config):
    return flat_config_options_impl(config, None)


def remove_reference_dir_from_path(path):
    if path.startswith('<'):
        closing_bracket = path.find('>')
        if closing_bracket == -1:
            raise ValueError(f'Invalid default value {path} for option')
        return '.' + path[closing_bracket + 1:]
    return path


def get_reference_dir_from_path(path):
    if path.startswith('<'):
        closing_bracket = path.find('>')
        if closing_bracket == -1:
            raise ValueError(f'Invalid default value {path} for option')
        return path[1:closing_bracket]
    return None


def escape_as_cpp_string(s):
    # Replace backslashes first to avoid escaping the escape characters added later
    s = s.replace("\\", "\\\\")
    # Replace double quotes
    s = s.replace("\"", "\\\"")
    # Replace single quotes
    s = s.replace("\'", "\\\'")
    # Replace newlines
    s = s.replace("\n", "\\n")
    # Replace carriage returns
    s = s.replace("\r", "\\r")
    # Replace tabs
    s = s.replace("\t", "\\t")
    return '"' + s + '"'


def to_cpp_value(option, v):
    t = option['type']
    n = option['name']
    if t == 'string':
        return f'std::string({escape_as_cpp_string(v)})'
    if t == 'enum':
        return f'{to_pascal_case(n)}::{to_pascal_case(v)}'

    if isinstance(v, str):
        return escape_as_cpp_string(v)
    if isinstance(v, bool):
        return 'true' if v else 'false'
    if isinstance(v, int):
        return str(v)
    if isinstance(v, list):
        return '{' + ', '.join([to_cpp_value(option, x) for x in v]) + '}'
    raise ValueError(f'Unsupported value type {type(v)}')


def indent(text, spaces):
    return '\n'.join(' ' * spaces + line for line in text.splitlines())


def indent_with(text, line_start_text):
    return '\n'.join(line_start_text + line for line in text.splitlines())


def to_cpp_type(option):
    has_suboptions = 'options' in option
    if has_suboptions:
        return f'{to_pascal_case(option["name"])}Options'

    option_type = option['type']
    if option_type == 'bool':
        return 'bool'
    if option_type == 'int':
        return 'int'
    if option_type == 'unsigned':
        return 'unsigned'
    if option_type in ['string', 'file-path', 'dir-path', 'path']:
        return 'std::string'
    if option_type == 'string-list':
        return 'StringList'
    if option_type in ['path-glob']:
        return 'PathGlobPattern'
    if option_type in ['symbol-glob']:
        return 'SymbolGlobPattern'
    if option_type == 'enum':
        option_values = option['values']
        cat_name = get_enum_category_name(option_values)
        return f'{to_pascal_case(cat_name)}'
    if option_type in ['list<string>', 'list<path>', 'list<file-path>', 'list<dir-path>']:
        return 'std::vector<std::string>'
    if option_type in ['list<path-glob>']:
        return 'std::vector<PathGlobPattern>'
    if option_type in ['list<symbol-glob>']:
        return 'std::vector<SymbolGlobPattern>'
    if option_type in ['map<string,string>']:
        return 'std::map<std::string, std::string>'
    if option_type in ['map<string,object>']:
        return 'std::map<std::string, dom::Object>'
    raise ValueError(f'to_cpp_type: Cannot convert option type {option_type} to C++ type')


def to_toolargs_type(option):
    has_suboptions = 'options' in option
    if has_suboptions:
        return f'{to_pascal_case(option["name"])}Options'

    option_type = option['type']
    if option_type == 'bool':
        return 'llvm::cl::opt<bool>'
    if option_type == 'int':
        return 'llvm::cl::opt<int>'
    if option_type == 'unsigned':
        return 'llvm::cl::opt<unsigned>'
    if option_type in ['string', 'file-path', 'dir-path', 'path', 'enum']:
        return 'llvm::cl::opt<std::string>'
    if option_type in ['string-list', 'list<string>', 'list<path>', 'list<file-path>', 'list<dir-path>', 'list<path-glob>', 'list<symbol-glob>']:
        return 'llvm::cl::list<std::string>'
    if option_type in ['map<string,string>']:
        return 'llvm::cl::list<std::string>'
    raise ValueError(f'to_cpp_type: Cannot convert option type {option_type} to C++ type')


def to_cpp_default_value(option, replace_reference_dir=None):
    has_suboptions = 'options' in option
    if has_suboptions:
        return None

    if replace_reference_dir is None:
        replace_reference_dir = True

    option_type = option['type']
    option_default = option['default']
    if option_type == 'bool':
        return 'false' if not option_default else 'true'
    if option_type in ['int', 'unsigned']:
        return str(option_default)
    if option_type == 'string':
        if not option_default:
            return None
        return f'"{option_default}"'
    if option_type in ['file-path', 'dir-path', 'path']:
        if not option_default:
            return None
        # Replace initial <xxx> from <xxx>/path/to/file path with "." if any
        if replace_reference_dir and option_default.startswith('<'):
            closing_bracket = option_default.find('>')
            if closing_bracket == -1:
                raise ValueError(f'Invalid default value {option_default} for option `{option["name"]}`')
            reference_path = option_default[1:closing_bracket]
            if reference_path == 'config-dir':
                option_default = '.' + option_default[closing_bracket + 1:]
            else:
                return None
        return f'"{option_default}"'
    if option_type == 'enum':
        option_values = option['values']
        cat_name = get_enum_category_name(option_values)
        return f'{to_pascal_case(cat_name)}::{to_pascal_case(option_default)}'
    if option_type in ['string-list', 'list<string>', 'list<path>', 'list<file-path>', 'list<dir-path>']:
        if not option_default:
            return None
        return '{' + ', '.join([f'"{str(s)}"' for s in option_default]) + '}'
    if option_type in ['list<symbol-glob>']:
        if not option_default:
            return None
        return '{' + ', '.join([f'SymbolGlobPattern::create("{str(s)}").value()' for s in option_default]) + '}'
    if option_type in ['list<path-glob>']:
        if not option_default:
            return None
        return '{' + ', '.join([f'PathGlobPattern::create("{str(s)}").value()' for s in option_default]) + '}'
    if option_type in ['map<string,string>']:
        if not option_default:
            return None
        return '{' + ', '.join([f'{{"{str(k)}", "{str(v)}"}}' for [k, v] in option_default.items()]) + '}'
    if option_type in ['map<string,object>']:
        # Free-form per-generator values are populated from the config DOM,
        # so there is no compile-time default to emit.
        return None
    raise ValueError(f'to_cpp_type: Cannot convert option type {option_type} to C++ type')


def generate_option_declaration(option, style=None):
    if style is None:
        style = 'cpp'
    if style not in ['cpp', 'toolargs']:
        raise ValueError(f'generate_option_declaration: Invalid style {style}')

    contents = ''

    option_name = option['name']
    option_brief = option['brief']
    option_details = option['details']
    has_suboptions = 'options' in option

    # Generate the struct for the suboptions
    if has_suboptions:
        # Generate the struct for the option
        contents += f'/** {option_brief}\n'
        contents += f' */\n'
        contents += f'struct {to_pascal_case(option_name)}Options {{\n'
        if style == 'toolargs':
            contents += f'    /// Constructor\n'
            contents += f'    {to_pascal_case(option_name)}Options(llvm::cl::OptionCategory& cat);\n\n'
        for suboption in option['options']:
            contents += indent(generate_option_declaration(suboption, style), 4)
            contents += '\n\n'
        contents += '};\n\n'

    # Generate doc comments
    brief_comment = text_wrap(f'/** {option_brief}', 70)
    brief_first_line = brief_comment.split('\n')[0]
    brief_rest_lines = '\n'.join(brief_comment.split('\n')[1:])
    brief_comment = f'{brief_first_line}\n{indent(brief_rest_lines, 4)}'
    # trim trailing spaces or line breaks
    brief_comment = brief_comment.strip(' \n')
    contents += f'{brief_comment}\n'
    if option_details:
        contents += f'\n'
        # Replace all '. ' with '. \n'
        option_details = option_details.replace('. ', '.\n\n').replace('/*', '/<star>')
        contents += f'{indent(text_wrap(option_details, 70), 4)}\n'
    contents += ' */\n'

    # Generate the declaration
    code_type = to_cpp_type(option) if style == 'cpp' else to_toolargs_type(option)
    contents += f'{code_type} {to_camel_case(option_name)}'
    if style == 'cpp' and option["type"] in ['enum', 'bool', 'unsigned', 'int']:
        # Enums should be initialized in the hpp file
        cpp_default_value = to_cpp_default_value(option, True)
        if cpp_default_value:
            contents += f' = {cpp_default_value}'
    contents += ';'
    return contents


def text_wrap(text, max_width):
    # Break the text into lines with a maximum width of max_width
    # The lines should obviously break at spaces
    original_lines = text.split('\n')
    lines = []
    for original_line in original_lines:
        words = original_line.split(' ')
        current_line = ''
        for word in words:
            if len(current_line) + len(word) + 1 > max_width:
                lines.append(current_line)
                current_line = word
            else:
                current_line = current_line + ' ' + word if current_line else word
        lines.append(current_line)

    return '\n'.join(lines)


def generate_config_info_inc(config):
    header_comment = generate_header_comment()
    option_inc_content = header_comment
    # Placeholder empty definitions
    option_inc_content += '// Placeholder empty definitions\n'
    option_inc_content += f'#ifndef OPTION\n'
    option_inc_content += f'#define OPTION(Name, Kebab, Desc)\n'
    option_inc_content += '#endif\n\n'
    for MACRO_NAME in ['CMDLINE_OPTION', 'COMMON_OPTION']:
        option_inc_content += f'#ifndef {MACRO_NAME}\n'
        option_inc_content += f'#define {MACRO_NAME}(Name, Kebab, Desc) OPTION(Name, Kebab, Desc)\n'
        option_inc_content += '#endif\n\n'
    option_inc_content += '#if !defined(INCLUDE_OPTION_OBJECTS) && !defined(INCLUDE_OPTION_NESTED)\n'
    option_inc_content += '#define INCLUDE_OPTION_NESTED\n'
    option_inc_content += '#endif\n\n'
    # Iterate configuration categories
    for category in config:
        category_name = category['category']
        # category_brief = category['brief']
        # category_details = category['details']

        # Define category
        option_inc_content += f'// {category_name}\n'

        # Iterate configuration options:
        for option in category['options']:
            option_name = option['name']
            option_brief = option['brief']
            # option_type = option['type']
            option_type_macro = 'COMMON_OPTION'.ljust(14)
            if category_name == 'Command Line Options':
                option_type_macro = 'CMDLINE_OPTION'.ljust(14)

            has_suboptions = 'options' in option
            if has_suboptions:
                # #ifndef INCLUDE_OPTION_NESTED
                option_inc_content += '#ifndef INCLUDE_OPTION_NESTED\n'
            option_inc_content += f'{option_type_macro}({to_camel_case(option_name).ljust(24)}, {option_name.ljust(24)}, {option_brief})\n'
            if has_suboptions:
                option_inc_content += '#else\n'
                flat_suboptions = get_flat_suboptions(option_name, option['options'])
                for suboption in flat_suboptions:
                    suboption_name = suboption['name']
                    suboption_brief = suboption['brief']
                    option_inc_content += f'COMMON_OPTION({to_camel_case(suboption_name).ljust(24)}, {suboption_name.ljust(24)}, {suboption_brief})\n'
                option_inc_content += '#endif\n'
        option_inc_content += '\n'
    # Undefine all macros
    option_inc_content += '// Undefine all macros\n'
    for MACRO_NAME in ['INCLUDE_OPTION_OBJECTS', 'INCLUDE_OPTION_NESTED', 'CMDLINE_OPTION', 'COMMON_OPTION', 'OPTION']:
        option_inc_content += f'#ifdef {MACRO_NAME}\n'
        option_inc_content += f'#undef {MACRO_NAME}\n'
        option_inc_content += '#endif\n\n'
    return option_inc_content


def generate_header_comment():
    header_comment = '/*\n'
    header_comment += ' * This file is generated automatically from the json file\n'
    header_comment += ' * `src/mrdocs/ConfigOptions.json`. Do not edit this file\n'
    header_comment += ' * manually. Instead, edit the json file and run the script\n'
    header_comment += ' * `utils/codegen/generate-config-info.py` to regenerate this file.\n'
    header_comment += ' */\n\n'
    return header_comment


def _include_base(source_dir, start_dir):
    # Path prefix used to build #include directives in the generated .cpp files,
    # pointing back at the private Support headers in the source tree. Prefer a
    # path relative to the generated file's directory, but fall back to the
    # absolute source path when no relative path exists: on Windows os.path.relpath
    # raises ValueError when the two paths are on different drives, which happens
    # during self-documentation, where the source is on the repo drive but the
    # mrdocs build tree is under %LOCALAPPDATA% on C:. Always use forward slashes
    # so the result is a valid #include string on every platform.
    try:
        base = os.path.relpath(source_dir, start_dir)
    except ValueError:
        base = os.path.abspath(source_dir)
    return base.replace(os.sep, '/')


def generate(config, output_dir, source_mrdocs_dir):
    print('Generating Configuration Information...')

    config = validate_and_normalize_config(config)

    mrdocs_build_include_dir = os.path.join(output_dir, 'include', 'mrdocs')
    if not os.path.exists(mrdocs_build_include_dir):
        os.makedirs(mrdocs_build_include_dir)

    # The schema is a single header now. The command line is parsed straight
    # from argv against this schema (see Config::load and
    # applyCommandLineOverrides), and `--help` is printed from the option
    # metadata the header carries, so there are no longer any generated
    # ConfigSchema.cpp or PublicToolArgs sources.
    config_schema_hpp = generate_config_schema_hpp(config)
    with open(os.path.join(mrdocs_build_include_dir, 'ConfigSchema.hpp'), 'w') as f:
        f.write(config_schema_hpp)


def main():
    # Read command line arguments
    input_file = sys.argv[1]
    output_dir = sys.argv[2]

    # ensure input is a json file
    if not input_file.endswith('.json'):
        print('Error: input file must be a json file')
        sys.exit(1)
    if os.path.exists(output_dir) and not os.path.isdir(output_dir):
        print('Error: output directory already exists')
        sys.exit(1)

    # parse input file
    with open(input_file, 'r') as f:
        config = json.load(f)

    generate(config, output_dir, os.path.dirname(os.path.abspath(input_file)))


if __name__ == "__main__":
    main()
