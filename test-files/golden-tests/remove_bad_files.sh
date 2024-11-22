#!/bin/bash
# This script finds and deletes all files with the extension ".bad.*" in the current directory and its subdirectories.
# It is useful for cleaning up unwanted or temporary files generated during testing or development.

# Use the find command to locate all files with the ".bad.*" extension and store them in an array.
# The -name option specifies the pattern to match.
# The -type f option ensures that only files are matched.
mapfile -t files < <(find . -name "*.bad.*" -type f)

# Count the number of files found.
file_count=${#files[@]}

# Delete the files.
for file in "${files[@]}"; do
    rm "$file"
done

# Print the number of files deleted.
echo "Deleted $file_count files."