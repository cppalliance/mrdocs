@echo off
REM
REM Licensed under the Apache License v2.0 with LLVM Exceptions.
REM See https://llvm.org/LICENSE.txt for license information.
REM SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
REM
REM Copyright (c) 2025 Alan de Freitas (alandefreitas@gmail.com)
REM
REM Official repository: https://github.com/cppalliance/mrdocs
REM

echo Building documentation UI
set "cwd=%CD%"
set "script_dir=%~dp0"
if not exist "%script_dir%ui\build\ui-bundle.zip" (
    echo Building antora-ui
    pushd "%script_dir%ui"
    call build.bat
    popd
)

echo Building documentation with Antora...
echo Installing npm dependencies...
call npm ci

echo Building docs in custom dir...
call npx antora --clean --fetch antora-playbook.yml
echo Done