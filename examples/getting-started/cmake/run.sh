#!/usr/bin/env bash
# Run from the project root. Extra arguments forward to mrdocs.
# Set the MRDOCS environment variable to override the binary
# (CI uses this to run against a freshly built mrdocs).
exec "${MRDOCS:-mrdocs}" --config=docs/mrdocs.yml "$@"
