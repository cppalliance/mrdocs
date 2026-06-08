#!/usr/bin/env bash
exec "${MRDOCS:-mrdocs}" --config=docs/mrdocs.yml "$@"
