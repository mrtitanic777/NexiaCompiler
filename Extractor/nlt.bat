@echo off
setlocal enabledelayedexpansion

:: --- HEADER ---
cls
echo -------------------------------------------------------
echo   NEXIA LIBRARY TOOL (NLT) v3.0 - Xbox 360 Researcher
echo -------------------------------------------------------
echo.

:: --- 1. AUTO-LOAD VISUAL STUDIO ENVIRONMENT ---
call :FIND_VS

:: --- 2. CONFIGURATION ---
set "SZ=C:\Program Files\7-Zip\7z.exe"

if "%~1"=="" (
    echo [INPUT] Please provide the library file.
    set /p "INPUT_PATH= >> Drag .lib file here and press Enter: "
    set "LIB_FILE=!INPUT_PATH:"=!"
) else (
    set "LIB_FILE=%~1"
)

if not exist "!LIB_FILE!" (
    echo [ERROR] File not found.
    pause
    exit /b 1
)

for %%F in ("!LIB_FILE!") do (
    set "ROOT_DIR=%%~dpnF_extracted"
)
set "FILES_DIR=!ROOT_DIR!\files"
set "SYMBOLS_DIR=!ROOT_DIR!\symbols"

echo.
echo [START] Target: !LIB_FILE!
echo [START] Output: !ROOT_DIR!

:: --- 3. EXTRACTION & DEEP SWAPPING ---
if exist "!ROOT_DIR!" rd /s /q "!ROOT_DIR!"
mkdir "!FILES_DIR!"
mkdir "!SYMBOLS_DIR!"

echo [7-ZIP] Extracting archive members...
"%SZ%" e "!LIB_FILE!" -o"!FILES_DIR!" * -y > nul

echo [CORE] Performing Full Header and Section Table Swap...
:: This logic flips Machine, Section Count, SymPtr, SymCount, AND all Section Headers
powershell -command "$files = Get-ChildItem '!FILES_DIR!\*.obj'; foreach($f in $files) { $b = [System.IO.File]::ReadAllBytes($f.FullName); if ($b.Count -gt 20 -and $b[0] -eq 0x01 -and $b[1] -eq 0xF2) { $b[0]=0xF2; $b[1]=0x01; $sections = [BitConverter]::ToUInt16($b, 2); $swSec = [uint16](($sections -band 0xFF) -shl 8 -bor ($sections -shr 8)); $sB = [BitConverter]::GetBytes($swSec); $b[2]=$sB[0]; $b[3]=$sB[1]; $symPtr = [BitConverter]::ToUInt32($b, 8); $swPtr = [uint32]([System.Net.IPAddress]::HostToNetworkOrder([int]$symPtr)); $pB = [BitConverter]::GetBytes($swPtr); $b[8]=$pB[0]; $b[9]=$pB[1]; $b[10]=$pB[2]; $b[11]=$pB[3]; $symCount = [BitConverter]::ToUInt32($b, 12); $swCnt = [uint32]([System.Net.IPAddress]::HostToNetworkOrder([int]$symCount)); $cB = [BitConverter]::GetBytes($swCnt); $b[12]=$cB[0]; $b[13]=$cB[1]; $b[14]=$cB[2]; $b[15]=$cB[3]; for($i=0; $i -lt $swSec; $i++) { $offset = 20 + ($i * 40); for($j=0; $j -lt 6; $j++) { $pos = $offset + 8 + ($j * 4); if ($pos + 4 -le $b.Count) { $val = [BitConverter]::ToUInt32($b, $pos); $swVal = [uint32]([System.Net.IPAddress]::HostToNetworkOrder([int]$val)); $vB = [BitConverter]::GetBytes($swVal); $b[$pos]=$vB[0]; $b[$pos+1]=$vB[1]; $b[$pos+2]=$vB[2]; $b[$pos+3]=$vB[3]; } } } [System.IO.File]::WriteAllBytes($f.FullName, $b); } }"

echo [PSHELL] Scraping strings and building Master Index...
set "MASTER_LOG=!ROOT_DIR!\MASTER_INDEX.txt"
echo NEXIA LIB TOOL MASTER INDEX > "!MASTER_LOG!"
echo -------------------------------------------------- >> "!MASTER_LOG!"

powershell -command "Get-ChildItem '!FILES_DIR!' | ForEach-Object { $fileName = $_.Name; $symFile = Join-Path '!SYMBOLS_DIR!' ($fileName + '.txt'); $matches = Select-String -Path $_.FullName -Pattern '[a-zA-Z0-9_]{5,}' -AllMatches | ForEach-Object { $_.Matches.Value } | Sort-Object -Unique; if ($matches) { $matches | Out-File -FilePath $symFile -Encoding utf8; $matches | ForEach-Object { \"[$fileName] $_\" } | Out-File -FilePath '!MASTER_LOG!' -Append -Encoding utf8 } }"

echo.
echo -------------------------------------------------------
echo   SUCCESS: Deep Extraction and Full Swapping complete.
echo -------------------------------------------------------
echo.
pause
exit /b

:FIND_VS
if defined VCINSTALLDIR exit /b
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath`) do set "VS_PATH=%%i"
if exist "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" (
    call "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x64 > nul
)
exit /b