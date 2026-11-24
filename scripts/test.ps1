# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [string]$Toolchain = "C:\tools\llvm-mingw-20261117-ucrt-x86_64",
    [string]$Filter = "*",
    [switch]$Sanitize
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$outputDirectory = Join-Path $root "build\tests"
New-Item -ItemType Directory -Force $outputDirectory | Out-Null

$dependencies = @{
    "test_gesture" = @("gesture.c", "actions.c")
    "test_config"  = @("config.c", "gesture.c", "actions.c", "tool.c", "style.c")
}

$linkerFlags = @{
    "test_scene" = @("-Wl,--wrap=realloc")
}

if ($Sanitize) {
    $env:PATH = "$Toolchain\bin;$env:PATH"
}

$failed = @()
$tests = Get-ChildItem tests -Filter "test_$Filter.c" | Sort-Object Name
foreach ($test in $tests) {
    $name = $test.BaseName
    if ($dependencies.ContainsKey($name)) {
        $sources = $dependencies[$name] | ForEach-Object { Join-Path "src" $_ }
    } else {
        $sources = Get-ChildItem src -Filter *.c | Where-Object { $_.Name -ne "main.c" } | ForEach-Object { $_.FullName }
    }
    $exe = Join-Path $outputDirectory "$name.exe"
    $extra = if ($linkerFlags.ContainsKey($name)) { $linkerFlags[$name] } else { @() }
    $sanitizerFlags = if ($Sanitize) { @("-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-fno-omit-frame-pointer") } else { @() }
    $arguments = $sanitizerFlags + @("-std=c11", "-Wall", "-Wextra", "-Werror", "-DUNICODE", "-D_UNICODE", "-D_WIN32_WINNT=0x0A00", "-O0", "-g", $test.FullName) + $sources + @("-o", $exe, "-luser32", "-lgdi32", "-lshell32", "-ladvapi32", "-lwtsapi32") + $extra
    & "$Toolchain\bin\clang.exe" @arguments
    if ($LASTEXITCODE -ne 0) {
        $failed += "$name (compile)"
        continue
    }
    & $exe
    if ($LASTEXITCODE -ne 0) {
        $failed += "$name (exit $LASTEXITCODE)"
    }
}

if ($failed.Count -gt 0) {
    Write-Host "FAILED: $($failed -join ', ')"
    exit 1
}
Write-Host "All $($tests.Count) test programs passed"
