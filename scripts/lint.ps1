# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Runs clang-tidy over src\ and reports findings.

.DESCRIPTION
Findings in files listed in scripts\lint-allowlist.txt are reported as known
debt and do not fail the run; anything else is an error. The allowlist may only
shrink: remove a file from it when its findings are fixed. With -Report the
script prints every finding and always exits 0.
#>
param(
    [string]$Toolchain = "C:\tools\llvm-mingw-20261117-ucrt-x86_64",
    [switch]$Report
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$tidy = Join-Path $Toolchain "bin\clang-tidy.exe"
if (-not (Test-Path $tidy)) {
    $found = Get-Command clang-tidy -ErrorAction SilentlyContinue
    if (-not $found) { throw "clang-tidy not found. Pass -Toolchain or put it on PATH." }
    $tidy = $found.Source
}
$allowFile = Join-Path $PSScriptRoot "lint-allowlist.txt"
$allowed = @()
if (Test-Path $allowFile) {
    $allowed = Get-Content $allowFile | Where-Object { $_ -and -not $_.StartsWith("#") }
}
$flags = @("--", "-std=c11", "-DUNICODE", "-D_UNICODE", "-D_WIN32_WINNT=0x0A00", "-Isrc", "--target=x86_64-w64-mingw32")
$sources = Get-ChildItem src -Filter *.c
$regression = @()
foreach ($source in $sources) {
    $lines = & $tidy --quiet $source.FullName @flags 2>$null | Where-Object { $_ -match "warning:|error:" }
    if (-not $lines) { continue }
    $own = $lines | Where-Object { $_ -match [regex]::Escape($source.Name) }
    if (-not $own) { continue }
    $known = $allowed -contains $source.Name
    foreach ($line in $own) { Write-Host ($(if ($known) { "[debt] " } else { "" }) + $line) }
    if (-not $known) { $regression += $source.Name }
}
if ($Report) { exit 0 }
if ($regression.Count -gt 0) {
    Write-Host "lint failed: $($regression | Sort-Object -Unique)"
    exit 1
}
Write-Host "lint clean (allowlisted files excluded)"
