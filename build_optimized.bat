@echo off
echo Building DataLeakGuard with optimizations for fast startup...

REM Clean build directory
if exist "build" rmdir /s /q build

REM Create build directory
mkdir build
cd build

REM Configure with optimizations
echo Configuring CMake with optimizations...
cmake .. ^
  -G "Visual Studio 17 2022" ^
  -A x64 ^
  -DDATALEAKGUARD_ENABLE_LTO=ON ^
  -DDATALEAKGUARD_AGGRESSIVE_OPTIMIZATIONS=ON ^
  -DDATALEAKGUARD_PREBAKE_UI_FONTS=ON ^
  -DDATALEAKGUARD_EMBED_FONT_GENERATOR=ON ^
  -DDATALEAKGUARD_SINGLE_BINARY=ON ^
  -DFLUXUI_ENABLE_MSAA=ON ^
  -DFLUXUI_ENABLE_VSYNC=ON ^
  -DFLUXUI_LOW_MEMORY=OFF ^
  -DFLUXUI_DEFAULT_BACKEND=VULKAN

if %errorlevel% neq 0 (
    echo CMake configuration failed!
    pause
    exit /b 1
)

echo Building Release configuration...
cmake --build . --config Release -- /m

if %errorlevel% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build completed successfully!
echo.
echo Optimized build flags:
echo - LTO (Link Time Optimization): ON
echo - Aggressive optimizations: ON
echo - Pre-baked fonts: ON
echo - Single binary: ON
echo - Fast startup config: Enabled
echo - Low-memory mode: OFF (keeps extra GPU buffers warm)
echo.
echo Application will start faster but may take longer to compile.
echo Executable: build\Release\DataLeakGuard.exe
echo.
pause
