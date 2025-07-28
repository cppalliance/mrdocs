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

REM Check for npm
where npm >nul 2>nul
if errorlevel 1 (
    echo npm is not installed
    exit /b 1
)

REM Check for npx
where npx >nul 2>nul
if errorlevel 1 (
    echo npx is not installed
    exit /b 1
)

REM Install modules if needed
if not exist node_modules (
    call npm ci
) else (
    for %%F in (package.json) do set package_json_time=%%~tF
    for %%F in (node_modules) do set node_modules_time=%%~tF
    if "%package_json_time%" GTR "%node_modules_time%" (
        call npm ci
    )
)

REM Lint
call npx gulp lint
if errorlevel 1 (
    set msg=Linting failed. Please run `npx gulp format` before pushing your code.
    if "%GITHUB_ACTIONS%"=="true" (
        echo ::error::%msg%
    ) else (
        echo %msg%
        call npx gulp format
    )
)

REM Build
call npx gulp