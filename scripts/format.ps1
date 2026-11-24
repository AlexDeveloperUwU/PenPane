# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Formats every C source and header with clang-format.

.DESCRIPTION
Without -Check the files are rewritten in place. With -Check nothing is written
and the script exits with 1 when any file would change, which is what CI uses.
#>
param(
    [string]$Toolchain = "C:\tools\llvm-mingw-20261117-ucrt-x86_64",
    [switch]$Check
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$formatter = Join-Path $Toolchain "bin\clang-format.exe"
if (-not (Test-Path $formatter)) {
    $found = Get-Command clang-format -ErrorAction SilentlyContinue
    if (-not $found) { throw "clang-format not found. Pass -Toolchain or put it on PATH." }
    $formatter = $found.Source
}
$files = Get-ChildItem src, tests, installer -Include *.c, *.h -Recurse | ForEach-Object { $_.FullName }
if ($Check) {
    & $formatter --dry-run --Werror @files
    exit $LASTEXITCODE
}
& $formatter -i @files
exit $LASTEXITCODE
