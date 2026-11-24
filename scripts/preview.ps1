# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [string]$Toolchain = "C:\tools\llvm-mingw-20261117-ucrt-x86_64",
    [string]$OutputDirectory = "build\preview"
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$sources = @("canvas", "raster", "render", "pen", "trail", "element", "geometry", "style", "mask_scratch", "text", "scene", "png", "grow") | ForEach-Object { "src\$_.c" }
$arguments = @("-std=c11", "-Wall", "-Wextra", "-O1", "-D_WIN32_WINNT=0x0A00", "-Isrc", "tests\render_preview.c") + $sources + @("-o", "build\render_preview.exe", "-luser32", "-lgdi32")
& "$Toolchain\bin\clang.exe" @arguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "build\render_preview.exe" $OutputDirectory
