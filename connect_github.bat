@echo off
set /p REPO_URL="Enter your GitHub Repository URL (e.g., https://github.com/username/AutoClicker.git): "

if "%REPO_URL%"=="" (
    echo Error: No URL provided.
    pause
    exit /b
)

git remote add origin %REPO_URL%
git branch -M main
git push -u origin main

echo.
echo If it asks for authentication, use a Personal Access Token or sign in via browser.
pause
