import json
import sys
import os
import argparse

# NotRequired was added in 3.11
if sys.version_info < (3, 11):
    from typing import Dict, Any

    Option = Dict[str, Any]
    OptionGroup = Dict[str, Any]
    SchemaType = Dict[str, Any]
else:
    from typing import TypedDict, NotRequired, Any

    Option = TypedDict(
        "Option",
        {
            "name": str,
            "command-line-only": NotRequired[bool],
            "brief": str,
            "details": NotRequired[str],
            "type": NotRequired[str],  # if absent, it's an object
            "default": NotRequired[str | int | list[int]],
            "required": NotRequired[bool],
            "values": NotRequired[list[str]],
            "options": NotRequired[list["Option"]],
            "min-value": NotRequired[int | float],
            "max-value": NotRequired[int | float],
        },
    )
    OptionGroup = TypedDict(
        "OptionGroup",
        {
            "category": str,
            "command-line-only": NotRequired[bool],
            "brief": str,
            "details": NotRequired[str],
            "options": list[Option],
        },
    )
    SchemaType = dict[str, Any]


def to_yaml_schema_type(option: Option) -> SchemaType:
    has_suboptions = "options" in option
    if has_suboptions or "type" not in option:
        return {"type": "object"}

    option_type = option["type"]
    if option_type == "bool":
        return {"type": "boolean", "enum": [True, False]}
    if option_type == "int":
        return {"type": "integer"}
    if option_type == "unsigned":
        return {"type": "integer", "minimum": 0}
    if option_type in ["string", "file-path", "dir-path", "path"]:
        return {"type": "string"}
    if option_type == "enum":
        assert "values" in option
        return {"enum": option["values"]}
    if option_type in [
        "list<string>",
        "list<path>",
        "list<file-path>",
        "list<dir-path>",
        "list<path-glob>",
        "list<symbol-glob>",
    ]:
        return {"type": "array", "items": {"type": "string"}}
    raise ValueError(
        f"to_yaml_schema_type: Cannot convert option type {option_type} to JSON/YAML schema type"
    )


def yaml_schema_required_props(options: list[Option]):
    return [opt["name"] for opt in options if "required" in opt and opt["required"]]


def to_yaml_schema_default_value(option: Option):
    has_suboptions = "options" in option
    if has_suboptions or "default" not in option:
        return None

    return option["default"]


def yaml_schema_property(option: Option):
    prop = to_yaml_schema_type(option)
    if "min-value" in option:
        prop["minimum"] = option["min-value"]
    if "max-value" in option:
        prop["maximum"] = option["max-value"]
    default_value = to_yaml_schema_default_value(option)
    if default_value is not None:
        prop["default"] = default_value
    prop["title"] = option["brief"]
    if "details" in option:
        prop["description"] = option["details"]
    if "options" in option:
        prop["properties"] = yaml_schema_properties(option["options"])
        prop["required"] = yaml_schema_required_props(option["options"])
    return prop


def yaml_schema_properties(options: list[Option]):
    props = {}
    for option in options:
        props[option["name"]] = yaml_schema_property(option)
    return props


def generate_yaml_schema(config: list[OptionGroup]):
    root = {
        # draft-07 is the newest supported one and we don't require any other features
        "$schema": "http://json-schema.org/draft-07/schema",
        "title": "MrDocs Configuration",
        "type": "object",
        "properties": {},
        "required": [],
    }

    for category in config:
        opts = [
            opt
            for opt in category["options"]
            if "command-line-only" not in opt or not opt["command-line-only"]
        ]
        root["properties"] |= yaml_schema_properties(opts)
        root["required"] += yaml_schema_required_props(opts)

    return json.dumps(root, indent=2, sort_keys=True)


def main():
    # Parse arguments
    parser = argparse.ArgumentParser(description="Generate or check the MrDocs YAML schema.")
    parser.add_argument('--output', type=str, help="Path to the output schema file.")
    parser.add_argument('--check', action='store_true', default=False,
                        help="Check if the generated schema matches the existing file.")
    args = parser.parse_args()

    # Determine the schema path
    mrdocs_root_dir = os.path.join(os.path.dirname(__file__), '..')
    default_schema_path = os.path.join(mrdocs_root_dir, 'docs', 'mrdocs.schema.json')
    mrdocs_schema_path = args.output if args.output else default_schema_path
    if not os.path.isabs(mrdocs_schema_path):
        mrdocs_schema_path = os.path.abspath(os.path.join(mrdocs_root_dir, mrdocs_schema_path))

    # Generate the schema
    mrdocs_config_path = os.path.join(mrdocs_root_dir, 'src', 'lib', 'ConfigOptions.json')
    with open(mrdocs_config_path, 'r') as f:
        config = json.loads(f.read())
    yaml_schema = generate_yaml_schema(config)

    if args.check:
        # Check if the generated schema matches the existing schema
        with open(mrdocs_schema_path, 'r') as f:
            existing_schema = f.read()
        if yaml_schema != existing_schema:
            print(
                "The generated schema does not match the existing schema."
                "Please run `util/generate-yaml-schema.py` to update the schema."
            )
            sys.exit(1)
        else:
            print("The generated schema matches the existing schema.")
    else:
        # Write the schema to the file
        with open(mrdocs_schema_path, 'w') as f:
            f.write(yaml_schema)


if __name__ == "__main__":
    main()
