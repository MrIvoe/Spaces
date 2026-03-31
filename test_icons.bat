@echo off
REM SimpleFences Icon Test Script
REM This script tests if SimpleFences is working with icon implementation

setlocal enabledelayedexpansion

set EXE_PATH=c:\Users\MrIvo\Github\IVOESimpleFences\IVOESimpleFences\build\bin\Debug\SimpleFences.exe
set TEST_FILE=%TEMP%\test_file.txt
set TEST_FOLDER=%TEMP%\test_folder

REM Create test files
echo Test content > "%TEST_FILE%"
if not exist "%TEST_FOLDER%" mkdir "%TEST_FOLDER%"

echo.
echo ========================================
echo SimpleFences Icon Implementation Test
echo ========================================
echo.
echo Test 1: Verify SimpleFences.exe exists
if exist "%EXE_PATH%" (
    echo [OK] SimpleFences.exe found
    for %%A in ("%EXE_PATH%") do (
        echo      File size: %%~zA bytes
        echo      Last modified: %%~TA
    )
) else (
    echo [FAIL] SimpleFences.exe not found at %EXE_PATH%
    exit /b 1
)

echo.
echo Test 2: Starting SimpleFences...
start "" "%EXE_PATH%"
timeout /t 3 /nobreak

echo.
echo Test 3: Application should now be running
echo.
echo Instructions for manual testing:
echo 1. Look for gray window(s) with SimpleFences title
echo 2. Open File Explorer
echo 3. Drag "%TEST_FILE%" into the fence window
echo 4. You should see an icon before the filename
echo 5. Try dragging "%TEST_FOLDER%" for folder icon
echo.
echo Cleanup test files when done:
echo   del "%TEST_FILE%"
echo   rmdir "%TEST_FOLDER%"
echo.
echo Test Complete!
pause
