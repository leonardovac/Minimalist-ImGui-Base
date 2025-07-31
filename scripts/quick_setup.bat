@echo off
chcp 65001 >nul

cd ..\

echo Setting up submodules...

echo [1/3] Updating submodules...
git submodule update --init --recursive
if %errorlevel% neq 0 (
    echo Error: Failed to update submodules
    pause
    exit /b 1
)

echo [2/3] Initializing sparse-checkout...
cd include\ImGui
if %errorlevel% neq 0 (
    echo Error: Could not navigate to include\ImGui directory
    pause
    exit /b 1
)

git sparse-checkout init --no-cone
if %errorlevel% neq 0 (
    echo Error: Failed to initialize sparse-checkout
    pause
    exit /b 1
)

echo [3/3] Setting sparse-checkout patterns...
git sparse-checkout set "/*" "!docs/" "!examples/" "!misc/" "!/imconfig.h" "!/imgui_demo.cpp" "!backends/sdlgpu3/" "!backends/vulkan/" "!backends/*_allegro5*" "!backends/*_android*" "!backends/*_glut*" "!backends/*_metal*" "!backends/*_osx*" "!backends/*_sdlgpu3*" "!backends/*_sdlrenderer*" "!backends/*_wgpu*"
if %errorlevel% neq 0 (
    echo Error: Failed to set sparse-checkout patterns
    pause
    exit /b 1
)

cd ..\..

echo Checking for vcpkg...
where vcpkg >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: vcpkg not found
    pause
    exit /b 1
)

echo Installing requirements...
echo [1/3] Installing Quill...
vcpkg install quill:x86-windows-static
vcpkg install quill:x64-windows-static
if %errorlevel% neq 0 (
    echo Error: Failed to install quill
    pause
    exit /b 1
)

echo [2/3] Installing SafetyHook...
vcpkg install safetyhook:x86-windows-static
vcpkg install safetyhook:x64-windows-static
if %errorlevel% neq 0 (
    echo Error: Failed to install safetyhook
    pause
    exit /b 1
)

echo [3/3] Installing Volk...
vcpkg install volk:x86-windows-static
vcpkg install volk:x64-windows-static
if %errorlevel% neq 0 (
    echo Error: Failed to install volk
    pause
    exit /b 1
)

echo.
echo ✓ Setup complete!
pause