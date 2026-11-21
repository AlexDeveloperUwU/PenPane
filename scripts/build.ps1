param(
    [string]$Toolchain = "C:\tools\llvm-mingw-20261117-ucrt-x86_64",
    [string]$Output = "build\ink.exe",
    [switch]$Debug
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
New-Item -ItemType Directory -Force (Split-Path $Output) | Out-Null
$sources = Get-ChildItem src -Filter *.c | ForEach-Object { $_.FullName }
$flags = if ($Debug) { @("-O0", "-g") } else { @("-Os", "-flto", "-s", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections") }
$arguments = @("-std=c11", "-Wall", "-Wextra", "-municode", "-mwindows", "-D_WIN32_WINNT=0x0A00") + $flags + $sources + @("-o", $Output, "-luser32", "-lgdi32", "-lshell32", "-ladvapi32", "-lwtsapi32")
& "$Toolchain\bin\clang.exe" @arguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
(Get-Item $Output).Length
