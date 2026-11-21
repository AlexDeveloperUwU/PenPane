param(
    [string]$Toolchain = "C:\tools\llvm-mingw-20261117-ucrt-x86_64",
    [string]$Output = "build\ink.exe",
    [switch]$Debug
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
New-Item -ItemType Directory -Force (Split-Path $Output) | Out-Null
$clang = "$Toolchain\bin\clang.exe"
$libraries = @("-luser32", "-lgdi32", "-lshell32", "-ladvapi32", "-lwtsapi32", "-limm32")
$common = @("-std=c11", "-Wall", "-Wextra", "-municode", "-D_WIN32_WINNT=0x0A00")

if ($Debug) {
    $sources = Get-ChildItem src -Filter *.c | ForEach-Object { $_.FullName }
    & $clang @($common + @("-mwindows", "-O0", "-g") + $sources + @("-o", $Output) + $libraries)
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    (Get-Item $Output).Length
    exit 0
}

$levels = @{ "raster" = "-Os"; "canvas" = "-O2" }
$objects = Join-Path (Split-Path $Output) "obj"
New-Item -ItemType Directory -Force $objects | Out-Null
Get-ChildItem $objects -Filter *.o | Remove-Item -Force
$compiled = @()
foreach ($source in Get-ChildItem src -Filter *.c) {
    $level = if ($levels.ContainsKey($source.BaseName)) { $levels[$source.BaseName] } else { "-Oz" }
    $object = Join-Path $objects ($source.BaseName + ".o")
    $flags = @($level, "-flto", "-ffunction-sections", "-fdata-sections", "-funwind-tables")
    & $clang @($common + $flags + @("-c", $source.FullName, "-o", $object))
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $compiled += $object
}
& $clang @(@("-municode", "-mwindows", "-flto", "-s", "-Wl,--gc-sections") + $compiled + @("-o", $Output) + $libraries)
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
(Get-Item $Output).Length


