@echo off
REM SimpleSpaces Icon Test Script
REM This script tests if SimpleSpaces is working with icon implementation

setlocal enabledelayedexpansion

set EXE_PATH=c:\Users\MrIvo\Github\IVOESimpleSpaces\IVOESimpleSpaces\build\bin\Debug\SimpleSpaces.exe
set TEST_FILE=%TEMP%\test_file.txt
set TEST_FOLDER=%TEMP%\test_folder

REM Create test files
echo Test content > "%TEST_FILE%"
if not exist "%TEST_FOLDER%" mkdir "%TEST_FOLDER%"

echo.
echo ========================================
echo SimpleSpaces Icon Implementation Test
echo ========================================
echo.
echo Test 1: Verify SimpleSpaces.exe exists
if exist "%EXE_PATH%" (
    echo [OK] SimpleSpaces.exe found
    for %%A in ("%EXE_PATH%") do (
        echo      File size: %%~zA bytes
        echo      Last modified: %%~TA
    )
) else (
    echo [FAIL] SimpleSpaces.exe not found at %EXE_PATH%
    exit /b 1
)

echo.
echo Test 2: Starting SimpleSpaces...
start "" "%EXE_PATH%"
timeout /t 3 /nobreak

echo.
echo Test 3: Application should now be running
echo.
echo Instructions for manual testing:
echo 1. Look for gray window(s) with SimpleSpaces title
echo 2. Open File Explorer
echo 3. Drag "%TEST_FILE%" into the space window
echo 4. You should see an icon before the filename
echo 5. Try dragging "%TEST_FOLDER%" for folder icon
echo.
echo Cleanup test files when done:
echo   del "%TEST_FILE%"
echo   rmdir "%TEST_FOLDER%"
echo.
echo Test Complete!
pause
