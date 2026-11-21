# SPDX-License-Identifier: GPL-3.0-or-later
param([string]$Root = "C:\tools")
$ErrorActionPreference = "Stop"
$version = "20261117"
$name = "llvm-mingw-$version-ucrt-x86_64"
$sha256 = "4D002E330A0F703625AA5CCBC37E96F8F824413E163BFC7336774AABD2C3A412"
$target = Join-Path $Root $name
if (Test-Path (Join-Path $target "bin\clang.exe")) { Write-Output $target; exit 0 }
New-Item -ItemType Directory -Force $Root | Out-Null
$zip = Join-Path $env:TEMP "$name.zip"
Invoke-WebRequest "https://github.com/mstorsjo/llvm-mingw/releases/download/$version/$name.zip" -OutFile $zip
$actual = (Get-FileHash $zip -Algorithm SHA256).Hash
if ($actual -ne $sha256) {
    Remove-Item $zip -Force
    throw "Checksum mismatch for $name.zip: expected $sha256 but got $actual. The download was deleted and nothing was extracted."
}
Expand-Archive $zip -DestinationPath $Root -Force
Write-Output $target
