#!/bin/bash
# This script finds and deletes all files with the extension ".bad.*" in the current directory and its subdirectories.

files=()
while IFS= read -r -d $'\0' file; do
    files+=("$file")
done < <(find . -name "*.bad.*" -type f -print0)

file_count=${#files[@]}

for file in "${files[@]}"; do
    rm "$file"
done

echo "Deleted $file_count files."