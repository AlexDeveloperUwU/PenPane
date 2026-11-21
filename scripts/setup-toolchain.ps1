param([string]$Root = "C:\tools")
$ErrorActionPreference = "Stop"
$version = "20261117"
$name = "llvm-mingw-$version-ucrt-x86_64"
$target = Join-Path $Root $name
if (Test-Path (Join-Path $target "bin\clang.exe")) { Write-Output $target; exit 0 }
New-Item -ItemType Directory -Force $Root | Out-Null
$zip = Join-Path $env:TEMP "$name.zip"
Invoke-WebRequest "https://github.com/mstorsjo/llvm-mingw/releases/download/$version/$name.zip" -OutFile $zip
Expand-Archive $zip -DestinationPath $Root -Force
Write-Output $target
