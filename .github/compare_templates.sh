#!/bin/bash

# This script compares the files in the Asciidoc and HTML directories and reports any discrepancies.

# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2024 Alan de Freitas (alandefreitas@gmail.com)
#
# Official repository: https://github.com/cppalliance/mrdocs
#

# Define variables for the directories
ASCIIDOC_DIR="share/mrdocs/addons/generator/asciidoc" # Asciidoc directory path
HTML_DIR="share/mrdocs/addons/generator/html" # HTML directory path

# Function to check for missing files
# Arguments:
#   src_dir: Source directory
#   dest_dir: Destination directory
#   src_ext: Source file extension
#   dest_ext: Destination file extension
check_missing_files() {
    local src_dir=$1
    local dest_dir=$2
    local src_ext=$3
    local dest_ext=$4
    local missing_files=()

    # Loop over each file in the source directory
    while IFS= read -r src_file; do
        relative_path="${src_file#$src_dir/}"
        dest_file="$dest_dir/${relative_path%$src_ext}$dest_ext"
        # If the file does not exist in the destination directory, add it to the missing_files array
        if [[ ! -f "$dest_file" ]]; then
            missing_files+=("${relative_path%$src_ext}$dest_ext")
        fi
    done < <(find "$src_dir" -type f -name "*$src_ext")

    # Print the missing files
    printf "%s\n" "${missing_files[@]}"
}

# Check for missing files in both directions
readarray -t missing_in_html < <(check_missing_files "$ASCIIDOC_DIR" "$HTML_DIR" ".adoc.hbs" ".html.hbs")
readarray -t missing_in_asciidoc < <(check_missing_files "$HTML_DIR" "$ASCIIDOC_DIR" ".html.hbs" ".adoc.hbs")

# Normalize the arrays to remove any empty elements
mapfile -t missing_in_html < <(printf "%s\n" "${missing_in_html[@]}" | sed '/^$/d')
mapfile -t missing_in_asciidoc < <(printf "%s\n" "${missing_in_asciidoc[@]}" | sed '/^$/d')

# If there are no missing files, print a success message
if [ ${#missing_in_html[@]} -eq 0 ] && [ ${#missing_in_asciidoc[@]} -eq 0 ]; then
    echo "All files match between the Asciidoc and HTML directories."
else
    # If there are missing files, prepare error messages
    html_message=""
    asciidoc_message=""

    if [ ${#missing_in_html[@]} -ne 0 ]; then
        html_message="The following files are missing from the HTML directory:\n$(printf "%s\n" "${missing_in_html[@]}" | sed 's/^/  - /')"
    fi

    if [ ${#missing_in_asciidoc[@]} -ne 0 ]; then
        asciidoc_message="The following files are missing from the Asciidoc directory:\n$(printf "%s\n" "${missing_in_asciidoc[@]}" | sed 's/^/  - /')"
    fi

    # If running in a CI environment, print a formatted warning message
    if [ -z "$CI" ] || [ -z "$GITHUB_ACTION" ]; then
        echo -e "HTML and Asciidoc templates do not match.\n$html_message\n$asciidoc_message"
    else
        final_message=$(echo -e "$html_message. $asciidoc_message" | tr -d '\n')
        echo -e "::warning title=HTML and Asciidoc templates do not match::$final_message"
    fi
fi