#!/bin/sh

# This script is used to build the Antora-UI

# Find npm and npx
npm_version=$(npm --version 2>/dev/null)
if [ -z "$npm_version" ]; then
  echo "npm is not installed"
  exit 1
fi
npx_version=$(npx --version 2>/dev/null)
if [ -z "$npx_version" ]; then
  echo "npx is not installed"
  exit 1
fi

# Install modules if we have to
node_modules_does_not_exist="[ ! -d 'node_modules' ]"
if $node_modules_does_not_exist; then
  npm ci
else
  package_json_mod_time="$(find package.json -prune -printf '%T@\n' | cut -d . -f 1)"
  node_modules_mod_time="$(find node_modules -prune -printf '%T@\n' | cut -d . -f 1)"
  package_json_is_newer="[ $package_json_mod_time -gt $node_modules_mod_time ]"
  if $package_json_is_newer; then
    npm ci
  fi
fi

# Lint
set -e
npx gulp lint
lint_exit_code=$?
if [ $lint_exit_code -ne 0 ]; then
  msg="Linting failed. Please run \`npx gulp format\` before pushing your code."
  if [ "$GITHUB_ACTIONS" = "true" ]; then
    echo "::error::$msg"
  else
    echo "$msg"
    npx gulp format
  fi
fi
set +e

# Build
npx gulp
