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

# Define variables for the directories
ASCIIDOC_DIR="share/mrdocs/addons/generator/asciidoc"
HTML_DIR="share/mrdocs/addons/generator/html"

# Function to check for missing files
check_missing_files() {
    local src_dir=$1
    local dest_dir=$2
    local src_ext=$3
    local dest_ext=$4
    local missing_files=()

    while IFS= read -r src_file; do
        relative_path="${src_file#$src_dir/}"
        dest_file="$dest_dir/${relative_path%$src_ext}$dest_ext"
        if [[ ! -f "$dest_file" ]]; then
            missing_files+=("${relative_path%$src_ext}$dest_ext")
        fi
    done < <(find "$src_dir" -type f -name "*$src_ext")

    printf "%s\n" "${missing_files[@]}"
}

missing_in_html=$(check_missing_files "$ASCIIDOC_DIR" "$HTML_DIR" ".adoc.hbs" ".html.hbs")
missing_in_asciidoc=$(check_missing_files "$HTML_DIR" "$ASCIIDOC_DIR" ".html.hbs" ".adoc.hbs")

if [ ${#missing_in_html[@]} -eq 0 ] && [ ${#missing_in_asciidoc[@]} -eq 0 ]; then
    echo "All files match between the Asciidoc and HTML directories."
else
    if [ -n "$missing_in_html" ]; then
        html_message="The following files are missing from the HTML directory:\n$(printf "%s\n" "$missing_in_html" | sed 's/^/  - /')"
    fi

    if [ -n "$missing_in_asciidoc" ]; then
        asciidoc_message="The following files are missing from the Asciidoc directory:\n$(printf "%s\n" "$missing_in_asciidoc" | sed 's/^/  - /')"
    fi

    if [ -z "$CI" ] || [ -z "$GITHUB_ACTION" ]; then
        echo -e "HTML and Asciidoc templates do not match.\n$html_message\n$asciidoc_message"
    else
        final_message=$(echo -e "$html_message. $asciidoc_message" | tr -d '\n')
        echo -e "::warning title=HTML and Asciidoc templates do not match::$final_message"
    fi
fi
