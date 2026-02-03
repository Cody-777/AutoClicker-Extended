@echo off
if not exist "bin" mkdir bin

echo Compiling Resources...
rc /fo bin\AutoClicker.res AutoClicker.rc
if %errorlevel% neq 0 (
    echo Resource compilation failed.
    exit /b 1
)

echo Compiling Application...
cl /EHsc /W3 /O2 /DUNICODE /D_UNICODE ^
    main.cpp AutoClicker.cpp ^
    bin\AutoClicker.res ^
    user32.lib gdi32.lib shell32.lib comctl32.lib ^
    /Fe:bin\AutoClicker.exe /link /SUBSYSTEM:WINDOWS

if %errorlevel% == 0 (
    echo.
    echo Build Successful! 
    echo Run bin\AutoClicker.exe to start.
) else (
    echo Build Failed!
)
pause
