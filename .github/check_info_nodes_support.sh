#!/bin/bash

#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

#
# This script is used to check that all Info nodes defined in
# include/Metadata/InfoNodes.inc are supported in all steps
# of MrDocs.
#
# Although most of that requirement is enforced in C++ source
# code, it is useful to check if these node types are also being
# included in templates and tests.
#

# Path of the current script
SCRIPT_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]}")"

# Directory of the current script
SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"

# Parent directory of the current script
MRDOCS_ROOT="$(dirname "$SCRIPT_DIR")"

# InfoNodes.inc path
INFO_NODES_INC="$MRDOCS_ROOT/include/mrdocs/Metadata/InfoNodes.inc"

# Determine if we should use github actions
if [ -n "$GITHUB_ACTIONS" ]; then
  GITHUB_ACTIONS=true
else
  GITHUB_ACTIONS=false
fi

# Function to print a message
# If GITHUB_ACTIONS is true, print the message in directed to >> $GITHUB_STEP_SUMMARY
# Otherwise, print the message to stdout
function print_message() {
  if $GITHUB_ACTIONS; then
    echo "$1" >> "$GITHUB_STEP_SUMMARY"
  else
    echo "$1"
  fi
}

# A function that calls print_message if print_message hasn't been called
# with that message already
declare -A print_message_once_messages
function print_message_once() {
      local message="$1"
      if [[ -z "${print_message_once_messages[$message]}" ]]; then
          print_message_once_messages[$message]=1
          echo "$message"
      fi
}

# Parse InfoNodes.inc
table_content=$(grep -E '^INFO\(' "$INFO_NODES_INC" | sed -E 's/^INFO\(([^)]*)\).*$/\1/')
# echo "$table_content"

print_message "# Info Nodes Support report"
while IFS= read -r line; do
  # Split the line into components using ',' as delimiter
  IFS=',' read -r -a components <<< "$line"

  # Access individual components
  name="${components[0]}"
  plural="${components[1]}"
  uppercase="${components[2]}"
  lowercase="${components[3]}"
  lowercase_plural="${components[4]}"
  description="${components[5]}"

  # Trim leading and trailing spaces from each component
  name=$(echo "$name" | xargs)
  plural=$(echo "$plural" | xargs)
  uppercase=$(echo "$uppercase" | xargs)
  lowercase=$(echo "$lowercase" | xargs)
  lowercase_plural=$(echo "$lowercase_plural" | xargs)
  description=$(echo "$description" | xargs)

  # Do something with the components
  # print_message "## Name: $name"
  # print_message "Plural: $plural"
  # print_message "Uppercase: $uppercase"
  # print_message "Lowercase: $lowercase"
  # print_message "Lowercase plural: $lowercase_plural"
  # print_message "Description: $description"

  # `include/mrdocs/Metadata/X.h` and `src/mrdocs/Metadata/X.cpp` should
  # be defined
  METADATA_INCLUDE_DIR="$MRDOCS_ROOT/include/mrdocs/Metadata"
  if [ ! -f "$METADATA_INCLUDE_DIR/$name.hpp" ]; then
    print_message_once "## ${name}Info"
    print_message "* include/mrdocs/Metadata/$name.h not found"
  fi

  # src/lib/AST/ASTVisitor.cpp should have a `build$name()` function
  # Look for the string `build$name(` in the file
  if ! grep -q "build$name(" "$MRDOCS_ROOT/src/lib/AST/ASTVisitor.cpp"; then
    print_message_once "## ${name}Info"
    print_message "* \`build$name()\` not found in \`src/lib/AST/ASTVisitor.cpp\`"
  fi

  # `src/lib/Gen/xml/XMLWriter.cpp` should define `XMLWriter::writeX()`
  # Just look for the string `write$name` in this file
  if ! grep -q "write$name" "$MRDOCS_ROOT/src/lib/Gen/xml/XMLWriter.cpp"; then
    print_message_once "## ${name}Info"
    print_message "* \`write$name\` not found in \`src/lib/Gen/xml/XMLWriter.cpp\`"
  fi

  # Function `void merge(${name}Info& I, ${name}Info& Other)` should be defined in
  # `src/lib/Metadata/DomMetadata.cpp`.
  # Like with the other function, we just use regex to look for this function,
  # ignoring consecutive whitespaces, newlines, leading and trailing spaces, and variable names.
  MERGE_REL_PATH="src/lib/Metadata/Reduce.cpp"
  MERGE_PATH="$MRDOCS_ROOT/$MERGE_REL_PATH"
  regex="void[[:space:]]+merge[[:space:]]*\(${name}Info[[:space:]]*&[[:space:]]*[a-zA-Z_][a-zA-Z0-9_]*[[:space:]]*,[[:space:]]*${name}Info[[:space:]]*&&[[:space:]]*[a-zA-Z_][a-zA-Z0-9_]*\)"
  if ! grep -Pzo "$regex" "$MERGE_PATH" > /dev/null; then
    print_message_once "## ${name}Info"
    print_message "The function \`void merge(${name}Info& I, ${name}Info&& Other)\` is not defined in \`$MERGE_REL_PATH\`."
  fi

  # `src/lib/Support/SafeNames.cpp` should have safe name support for $name
  # Look the string matching the regex `"\d+$lowercase"` in the file.
  # Note: the regex is looking for a string that literally quoted.
  SAFE_NAMES_REL_PATH="src/lib/Support/SafeNames.cpp"
  SAFE_NAMES_PATH="$MRDOCS_ROOT/$SAFE_NAMES_REL_PATH"
  regex="\"[0-9]+${lowercase}\""
  if ! grep -qE "$regex" "$SAFE_NAMES_PATH"; then
    print_message_once "## ${name}Info"
    print_message "* \`$regex\` not found in \`$SAFE_NAMES_REL_PATH\`"
  fi

  # `include/mrdocs/mrdocs.natvis` should include the ${name}Info type
  # We look for the string `<AlternativeType Name="clang::mrdocs::${name}Info"/>` in the file
  if ! grep -q "<AlternativeType Name=\"clang::mrdocs::${name}Info\"/>" "$MRDOCS_ROOT/include/mrdocs/mrdocs.natvis"; then
    print_message_once "## ${name}Info"
    print_message "* \`<AlternativeType Name=\"clang::mrdocs::${name}Info\"/>\` not found in \`include/mrdocs/mrdocs.natvis\`"
  fi

  # `mrdocs.rnc` should include the type ${name} in multiple places
  # At least look for the string ${name} in the file
  if ! grep -q "${name}" "$MRDOCS_ROOT/mrdocs.rnc"; then
    print_message_once "## ${name}Info"
    print_message "* \`${name}\` not found in \`mrdocs.rnc\`"
  fi

  # There should be template files for each info type
  # `share/mrdocs/addons/generator/asciidoc/partials/signature/${lowercase}.adoc.hbs`
  GENERATOR_REL_DIR="share/mrdocs/addons/generator"
  ASCIIDOC_REL_DIR="$GENERATOR_REL_DIR/asciidoc"
  ASCIIDOC_SIGNATURE_REL_DIR="$ASCIIDOC_REL_DIR/partials/signature"
  ASCIIDOC_SIGNATURE_DIR="$MRDOCS_ROOT/$ASCIIDOC_SIGNATURE_REL_DIR"
  if [ ! -f "$ASCIIDOC_SIGNATURE_DIR/${lowercase}.adoc.hbs" ]; then
    print_message_once "## ${name}Info"
    print_message "* \`$ASCIIDOC_SIGNATURE_REL_DIR/${lowercase}.adoc.hbs\` not found"
  fi
  # `share/mrdocs/addons/generator/asciidoc/partials/symbols/${lowercase}.adoc.hbs`
  ASCIIDOC_SYMBOLS_REL_DIR="$ASCIIDOC_REL_DIR/partials/symbols"
  ASCIIDOC_SYMBOLS_DIR="$MRDOCS_ROOT/$ASCIIDOC_SYMBOLS_REL_DIR"
  if [ ! -f "$ASCIIDOC_SYMBOLS_DIR/${lowercase}.adoc.hbs" ]; then
    print_message_once "## ${name}Info"
    print_message "* \`$ASCIIDOC_SYMBOLS_REL_DIR/${lowercase}.adoc.hbs\` not found"
  fi
  # `share/mrdocs/addons/generator/html/partials/signature/${lowercase}.html.hbs`
  HTML_REL_DIR="$GENERATOR_REL_DIR/html"
  HTML_SIGNATURE_REL_DIR="$HTML_REL_DIR/partials/signature"
  HTML_SIGNATURE_DIR="$MRDOCS_ROOT/$HTML_SIGNATURE_REL_DIR"
  if [ ! -f "$HTML_SIGNATURE_DIR/${lowercase}.html.hbs" ]; then
    print_message_once "## ${name}Info"
    print_message "* \`$HTML_SIGNATURE_REL_DIR/${lowercase}.html.hbs\` not found"
  fi
  # `share/mrdocs/addons/generator/html/partials/symbols/${lowercase}.html.hbs`
  HTML_SYMBOLS_REL_DIR="$HTML_REL_DIR/partials/symbols"
  HTML_SYMBOLS_DIR="$MRDOCS_ROOT/$HTML_SYMBOLS_REL_DIR"
  if [ ! -f "$HTML_SYMBOLS_DIR/${lowercase}.html.hbs" ]; then
    print_message_once "## ${name}Info"
    print_message "* \`$HTML_SYMBOLS_REL_DIR/${lowercase}.html.hbs\` not found"
  fi

  # We want to know if
  # `share/mrdocs/addons/generator/asciidoc/partials/symbols/tranche.adoc.hbs`
  # includes the ${name}Info type somewhere as `members=tranche.${lowercase_plural}`
  ASCIIDOC_PARTIALS_REL_DIR="$ASCIIDOC_REL_DIR/partials"
  ASCIIDOC_PARTIALS_DIR="$MRDOCS_ROOT/$ASCIIDOC_PARTIALS_REL_DIR"
  if ! grep -q "tranche.${lowercase_plural}" "$ASCIIDOC_PARTIALS_DIR/tranche.adoc.hbs"; then
    INCLUDED_IN_ASCIIDOC_TRANCHE="true"
  else
    INCLUDED_IN_ASCIIDOC_TRANCHE="false"
  fi
  # We want to know if
  # `share/mrdocs/addons/generator/html/partials/symbols/tranche.html.hbs`
  # includes the ${name}Info type somewhere as `{{>info-list tranche.${lowercase_plural}}}`
  HTML_PARTIALS_REL_DIR="$HTML_REL_DIR/partials"
  HTML_PARTIALS_DIR="$MRDOCS_ROOT/$HTML_PARTIALS_REL_DIR"
  if ! grep -q "{{>info-list tranche.${lowercase_plural}}" "$HTML_PARTIALS_DIR/tranche.html.hbs"; then
    INCLUDED_IN_HTML_TRANCHE="true"
  else
    INCLUDED_IN_HTML_TRANCHE="false"
  fi

  # Check if it's included in one tranche but not the other
  if [ "$INCLUDED_IN_ASCIIDOC_TRANCHE" = "true" ] && [ "$INCLUDED_IN_HTML_TRANCHE" = "false" ]; then
    print_message_once "## ${name}Info"
    print_message "* \`members=tranche.${lowercase_plural}\` not included in \`$ASCIIDOC_PARTIALS_REL_DIR/tranche.adoc.hbs\` but included in \`tranche.html.hbs\`"
  fi
  if [ "$INCLUDED_IN_HTML_TRANCHE" = "true" ] && [ "$INCLUDED_IN_ASCIIDOC_TRANCHE" = "false" ]; then
    print_message_once "## ${name}Info"
    print_message "* \`{{>info-list tranche.${lowercase_plural}}\` not included in \`$HTML_PARTIALS_REL_DIR/tranche.html.hbs\` but included in \`tranche.adoc.hbs\`"
  fi

done <<< "$table_content"

# Check if $print_message_once_messages is empty
if [ ${#print_message_once_messages[@]} -eq 0 ]; then
  print_message_once "No issues found"
fi
