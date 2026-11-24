# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Bumps the PenPane version everywhere it is stored and opens a CHANGELOG entry.

.DESCRIPTION
Reads the current version from src\version.h, computes the next one, rewrites
src\version.h and Doxyfile, and inserts a dated, empty section at the top of
CHANGELOG.md. It never commits, tags or pushes.

.EXAMPLE
scripts\bump.ps1 patch
scripts\bump.ps1 minor -DryRun
#>
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("major", "minor", "patch")]
    [string]$Part,
    [switch]$DryRun
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Read-Text([string]$path) {
    [System.IO.File]::ReadAllText($path, $utf8)
}

function Write-Text([string]$path, [string]$text) {
    if ($DryRun) { Write-Host "would write $path"; return }
    [System.IO.File]::WriteAllText($path, $text, $utf8)
}

function Set-Once([string]$text, [string]$pattern, [string]$replacement, [string]$label) {
    $regex = New-Object System.Text.RegularExpressions.Regex($pattern, "Multiline")
    if (-not $regex.IsMatch($text)) { throw "Pattern for $label not found." }
    $regex.Replace($text, $replacement, 1)
}

$versionPath = Join-Path $root "src\version.h"
$version = Read-Text $versionPath
$current = @{}
foreach ($name in "MAJOR", "MINOR", "PATCH") {
    $match = [regex]::Match($version, "#define PENPANE_VERSION_$name (\d+)")
    if (-not $match.Success) { throw "PENPANE_VERSION_$name not found in version.h." }
    $current[$name] = [int]$match.Groups[1].Value
}
$old = "$($current.MAJOR).$($current.MINOR).$($current.PATCH)"

switch ($Part) {
    "major" { $current.MAJOR++; $current.MINOR = 0; $current.PATCH = 0 }
    "minor" { $current.MINOR++; $current.PATCH = 0 }
    "patch" { $current.PATCH++ }
}
$new = "$($current.MAJOR).$($current.MINOR).$($current.PATCH)"

$changelogPath = Join-Path $root "CHANGELOG.md"
$changelog = Read-Text $changelogPath
if ($changelog -match [regex]::Escape("## [$new]")) { throw "CHANGELOG already has $new." }

foreach ($name in "MAJOR", "MINOR", "PATCH") {
    $version = Set-Once $version "^#define PENPANE_VERSION_$name \d+" "#define PENPANE_VERSION_$name $($current[$name])" "version $name"
}
$version = Set-Once $version '^#define PENPANE_VERSION_STRING ".*"' "#define PENPANE_VERSION_STRING `"$new`"" "version string"
Write-Text $versionPath $version

$doxyPath = Join-Path $root "Doxyfile"
$doxy = Set-Once (Read-Text $doxyPath) "^PROJECT_NUMBER\s*=.*" "PROJECT_NUMBER         = $new" "Doxyfile"
Write-Text $doxyPath $doxy

$date = Get-Date -Format "yyyy-MM-dd"
$entry = "## [$new] - $date`n`n### Added`n`n### Changed`n`n### Fixed`n`n"
$changelog = Set-Once $changelog "^## \[" ($entry.Replace('$', '$$') + "## [") "CHANGELOG anchor"
Write-Text $changelogPath $changelog

Write-Host "$old -> $new"
Write-Host "Fill in CHANGELOG.md (delete empty sections), then build, test, commit and push."
