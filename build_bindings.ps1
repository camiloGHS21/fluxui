# ==============================================================================
#  FluxUI Language Bindings Build Script (C++, Rust, Zig)
# ==============================================================================
#  This script compiles the FluxUI shared C++ core, then generates and builds
#  the corresponding Rust and Zig bindings libraries and executable examples.
# ==============================================================================

$ErrorActionPreference = "Stop"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  FluxUI Language Bindings Builder" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# ── 1. Compile C++ Core Libraries and Game Demos ─────────────────────────────
Write-Host "`n[1/4] Configuring and Compiling C++ Core Shared Libraries..." -ForegroundColor Yellow
cmake -B build -DFLUXUI_BUILD_SHARED=ON
cmake --build build --config Release

# Ensure build/Release exists
if (-not (Test-Path "build/Release")) {
    New-Item -ItemType Directory -Path "build/Release" | Out-Null
}

# ── 2. Build Rust Bindings and Examples ───────────────────────────────────────
Write-Host "`n[2/4] Compiling Rust FFI Bindings Library and Examples..." -ForegroundColor Yellow
$CargoBin = "C:\Users\Administrator\.cargo\bin"
$Rustc = "$CargoBin\rustc.exe"

if (-not (Test-Path $Rustc)) {
    Write-Error "rustc.exe not found at $Rustc. Please ensure Rust is installed."
}

Write-Host "  -> Compiling rustc library 'fluxui.rlib'..." -ForegroundColor Gray
& $Rustc --crate-name fluxui --crate-type rlib bindings/rust/lib.rs `
  -L native=build/Release -l dylib=fluxui_shared `
  -o build/Release/libfluxui.rlib

Write-Host "  -> Compiling Rust minimal example 'minimal_rust.exe'..." -ForegroundColor Gray
& $Rustc examples/rust/minimal.rs `
  -L native=build/Release -l dylib=fluxui_shared `
  -o build/Release/minimal_rust.exe `
  --extern fluxui=build/Release/libfluxui.rlib

# ── 3. Build Zig Bindings and Examples ───────────────────────────────────────
Write-Host "`n[3/4] Compiling Zig FFI Bindings and Examples..." -ForegroundColor Yellow

# Copy the bindings definition locally to avoid module path restriction errors
Write-Host "  -> Syncing Zig bindings file..." -ForegroundColor Gray
Copy-Item "bindings/zig/fluxui.zig" "examples/zig/fluxui.zig" -Force

Write-Host "  -> Compiling Zig minimal example 'minimal_zig.exe'..." -ForegroundColor Gray
zig build-exe examples/zig/minimal.zig `
  -I fluxui/include -L build/Release -lfluxui_shared `
  --name minimal_zig

# Move output file and PDB to build/Release
Move-Item "minimal_zig.exe" "build/Release/minimal_zig.exe" -Force -ErrorAction SilentlyContinue
Move-Item "minimal_zig.pdb" "build/Release/minimal_zig.pdb" -Force -ErrorAction SilentlyContinue

# ── 4. Verify Build and Display Results ───────────────────────────────────────
Write-Host "`n[4/4] Verifying Build Outputs..." -ForegroundColor Yellow

$Outputs = @(
    "build/Release/fluxui_shared.dll",
    "build/Release/game_2d.exe",
    "build/Release/game_3d.exe",
    "build/Release/libfluxui.rlib",
    "build/Release/minimal_rust.exe",
    "build/Release/minimal_zig.exe"
)

$AllSucceeded = $true
foreach ($Out in $Outputs) {
    if (Test-Path $Out) {
        Write-Host "  [OK]  $Out" -ForegroundColor Green
    } else {
        Write-Host "  [ERR] Missing: $Out" -ForegroundColor Red
        $AllSucceeded = $false
    }
}

Write-Host "`n==========================================================" -ForegroundColor Cyan
if ($AllSucceeded) {
    Write-Host "  SUCCESS: All targets compiled and verified successfully!" -ForegroundColor Green
    Write-Host "  Run them from 'build/Release/'." -ForegroundColor Gray
} else {
    Write-Host "  FAILURE: Some targets failed to compile." -ForegroundColor Red
}
Write-Host "==========================================================" -ForegroundColor Cyan
