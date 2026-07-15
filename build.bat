@echo off
echo.
echo  NexiaCompiler v2.0 - MinGW Build
echo  ==========================================
echo.

REM Check for g++
where g++ >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo  ERROR: g++ not found!
    echo.
    echo  You need MinGW-w64 installed and on your PATH.
    echo.
    echo  Quick install options:
    echo    1. WinLibs: https://winlibs.com
    echo       Download, extract, add the bin\ folder to PATH
    echo.
    echo    2. MSYS2: https://www.msys2.org
    echo       Then run: pacman -S mingw-w64-x86_64-gcc
    echo.
    echo  To add to PATH:
    echo    Settings ^> System ^> About ^> Advanced System Settings
    echo    ^> Environment Variables ^> Path ^> Edit ^> New
    echo    Add: C:\mingw64\bin  (or wherever your g++.exe is)
    echo.
    pause
    exit /b 1
)

REM Check for mingw32-make
where mingw32-make >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo  WARNING: mingw32-make not found, trying 'make'...
    where make >nul 2>&1
    if %ERRORLEVEL% neq 0 (
        echo  ERROR: No make tool found!
        echo  mingw32-make should come with your MinGW install.
        pause
        exit /b 1
    )
    set MAKE=make
) else (
    set MAKE=mingw32-make
)

echo  Found: g++ and %MAKE%
echo.

REM Build
%MAKE% -f Makefile all
if %ERRORLEVEL% neq 0 (
    echo.
    echo  Build failed! Check the errors above.
    pause
    exit /b 1
)

pause
