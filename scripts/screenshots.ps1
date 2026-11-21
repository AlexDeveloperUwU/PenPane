# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Regenerates the images used by the README into docs\images.

.DESCRIPTION
Renders the sample drawings with tests\render_preview.c through scripts\preview.ps1, then starts the
user interface preview program and captures each floating panel from the screen. Needs an interactive
desktop session. Both programs are built here from the sources.

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\screenshots.ps1
#>
param(
    [string]$Toolchain = "C:\tools\llvm-mingw-20261117-ucrt-x86_64",
    [string]$OutputDirectory = "docs\images"
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class PenPaneWindows {
    public delegate bool EnumProc(IntPtr window, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr parameter);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr window, out Rect rectangle);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr window, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    public static List<Rect> Find(string className) {
        var found = new List<Rect>();
        EnumWindows((window, parameter) => {
            var text = new StringBuilder(256);
            GetClassNameW(window, text, 256);
            Rect rectangle;
            if (text.ToString() == className && IsWindowVisible(window) && GetWindowRect(window, out rectangle)) { found.Add(rectangle); }
            return true;
        }, IntPtr.Zero);
        return found;
    }
}
"@
[void][PenPaneWindows]::SetProcessDPIAware()

New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
& "$PSScriptRoot\preview.ps1" -Toolchain $Toolchain
$sampleSources = @{
    "sample.png" = "sample-drawing.png"
    "shapes.png" = "shapes.png"
    "pens.png" = "pen-strokes.png"
    "trails.png" = "laser-and-eraser-trails.png"
}
foreach ($name in $sampleSources.Keys) {
    $image = [System.Drawing.Image]::FromFile((Resolve-Path (Join-Path "build\preview" $name)).Path)
    $image.Save((Join-Path (Resolve-Path $OutputDirectory) $sampleSources[$name]), [System.Drawing.Imaging.ImageFormat]::Png)
    $image.Dispose()
}

$sources = @("tests\ui_preview.c") + (Get-ChildItem src -Filter *.c | Where-Object { $_.Name -ne "main.c" } | ForEach-Object { $_.FullName })
$arguments = @("-std=c11", "-O1", "-D_WIN32_WINNT=0x0A00", "-DUNICODE", "-D_UNICODE", "-municode", "-mwindows", "-Isrc") + $sources + @("-o", "build\ui_preview.exe", "-luser32", "-lgdi32", "-lshell32", "-ladvapi32", "-lwtsapi32", "-limm32")
& "$Toolchain\bin\clang.exe" @arguments
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$margin = 6
$process = Start-Process "build\ui_preview.exe" -ArgumentList @("--tool", "5", "--lock") -PassThru
try {
    Start-Sleep -Seconds 3
    $windows = [PenPaneWindows]::Find("PenPaneIsland")
    if ($windows.Count -ne 3) { throw "Expected three panels but found $($windows.Count)." }
    $sorted = $windows | Sort-Object { ($_.Right - $_.Left) * ($_.Bottom - $_.Top) } -Descending
    $toolbar = $windows | Sort-Object { ($_.Right - $_.Left) - ($_.Bottom - $_.Top) } -Descending | Select-Object -First 1
    $properties = $windows | Where-Object { $_.Left -ne $toolbar.Left -or $_.Top -ne $toolbar.Top } | Sort-Object { $_.Bottom - $_.Top } -Descending | Select-Object -First 1
    $history = $windows | Where-Object { ($_.Left -ne $toolbar.Left -or $_.Top -ne $toolbar.Top) -and ($_.Left -ne $properties.Left -or $_.Top -ne $properties.Top) } | Select-Object -First 1
    $targets = @{ "toolbar.png" = $toolbar; "properties-panel.png" = $properties; "history-panel.png" = $history }
    foreach ($name in $targets.Keys) {
        $rect = $targets[$name]
        $width = $rect.Right - $rect.Left + 2 * $margin
        $height = $rect.Bottom - $rect.Top + 2 * $margin
        $bitmap = New-Object System.Drawing.Bitmap $width, $height
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CopyFromScreen($rect.Left - $margin, $rect.Top - $margin, 0, 0, (New-Object System.Drawing.Size $width, $height))
        $graphics.Dispose()
        $bitmap.Save((Join-Path (Resolve-Path $OutputDirectory) $name), [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
    }
} finally {
    if (-not $process.HasExited) { $process.Kill() }
}
Get-ChildItem $OutputDirectory | ForEach-Object { "{0} {1}" -f $_.Name, $_.Length }
