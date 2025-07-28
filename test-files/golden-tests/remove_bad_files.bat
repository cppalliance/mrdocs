@echo off
REM This script deletes all files matching *.bad.* in the current directory and subdirectories.

setlocal enabledelayedexpansion
set count=0

for /r %%F in (*.bad.*) do (
    del "%%F"
    set /a count+=1
)

echo Deleted !count! files.
endlocal