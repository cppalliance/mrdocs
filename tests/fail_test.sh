#!/bin/sh

if $@; then
  echo "Command '$@' didn't fail."
  exit 1
else
  exit 0
fi