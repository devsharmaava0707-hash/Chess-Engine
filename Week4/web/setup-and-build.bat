@echo off
setlocal
rem One-time bootstrap for Windows. Requires Git and an internet connection.
set "WEB_DIR=%~dp0"
set "SDK_DIR=%WEB_DIR%.tools\emsdk"

where git >nul 2>nul
if errorlevel 1 (
  echo ERROR: Git was not found. Install Git for Windows, then run this again.
  exit /b 1
)

if not exist "%SDK_DIR%\emsdk.bat" (
  if not exist "%WEB_DIR%.tools" mkdir "%WEB_DIR%.tools"
  echo Downloading the Emscripten SDK. This is a one-time download.
  git clone https://github.com/emscripten-core/emsdk.git "%SDK_DIR%"
  if errorlevel 1 exit /b 1
)

call "%SDK_DIR%\emsdk.bat" install latest
if errorlevel 1 exit /b 1
call "%SDK_DIR%\emsdk.bat" activate latest
if errorlevel 1 exit /b 1
call "%SDK_DIR%\emsdk_env.bat"
if errorlevel 1 exit /b 1

cd /d "%WEB_DIR%"
call build.bat
if errorlevel 1 exit /b 1

echo.
echo Build finished. Start a local server with:
echo   python -m http.server 8000 --directory public
echo Then open http://localhost:8000
