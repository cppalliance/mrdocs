#!/usr/bin/env bash
exec "${MRDOCS:-mrdocs}" --config=mrdocs.yml "$@"
