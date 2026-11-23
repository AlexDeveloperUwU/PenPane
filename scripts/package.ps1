# SPDX-License-Identifier: GPL-3.0-or-later
param(
    [string]$Toolchain = "C:\tools\llvm-mingw-20261117-ucrt-x86_64",
    [string]$OutDir = "build"
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

$program = "$OutDir\PenPane.exe"
& "$PSScriptRoot\build.ps1" -Toolchain $Toolchain -Output $program
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$version = (Select-String -Path src\version.h -Pattern 'define PENPANE_VERSION_STRING "([^"]+)"').Matches[0].Groups[1].Value
$resource = "$OutDir\obj\setup_resource.o"
$output = "$OutDir\PenPane-Setup-$version.exe"
Get-ChildItem $OutDir -Filter "PenPane-Setup-*.exe*" | Remove-Item -Force

& "$Toolchain\bin\llvm-windres.exe" "-I" "res" "-I" "src" "-I" "installer" "-I" $OutDir "-I" "." "-O" "coff" "installer\setup.rc" "-o" $resource
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "$Toolchain\bin\clang.exe" -std=c11 -Wall -Wextra -municode -mwindows -D_WIN32_WINNT=0x0A00 -Oz -s "-Wl,--gc-sections" -I src installer\setup.c $resource -o $output -lshell32 -lole32 -luuid -ladvapi32 -luser32 -lcomctl32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

foreach ($file in @($output, $program)) {
    $name = Split-Path $file -Leaf
    $hash = (Get-FileHash $file -Algorithm SHA256).Hash.ToLower()
    [System.IO.File]::WriteAllText("$PWD\$file.sha256", "$hash  $name`n")
}
(Get-Item $output).Length
