# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Generates res\penpane.ico, a multi-size icon of a pen nib on a pane, from vector geometry.

.DESCRIPTION
Every size is drawn on its own at eight times the resolution and reduced with a high quality filter,
so small sizes stay crisp. Each image is stored as PNG inside the ICO container. The geometry matches
the pen glyph the application draws for the tray icon (32 design units).

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\make-icon.ps1
#>
param([string]$Output = "res\penpane.ico")
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$sizes = @(16, 24, 32, 48, 256)
$supersample = 8
$pane = [System.Drawing.Color]::FromArgb(255, 0x40, 0x3E, 0x6A)
$border = [System.Drawing.Color]::FromArgb(255, 0xA8, 0xA5, 0xFF)
$pen = [System.Drawing.Color]::FromArgb(255, 0xE0, 0xDF, 0xFF)

function Add-Capsule($graphics, $brush, $unit, $x1, $y1, $x2, $y2, $r1, $r2) {
    $dx = $x2 - $x1; $dy = $y2 - $y1
    $length = [math]::Sqrt($dx * $dx + $dy * $dy)
    $nx = -$dy / $length; $ny = $dx / $length
    $points = @(
        [System.Drawing.PointF]::new([single](($x1 + $nx * $r1) * $unit), [single](($y1 + $ny * $r1) * $unit)),
        [System.Drawing.PointF]::new([single](($x2 + $nx * $r2) * $unit), [single](($y2 + $ny * $r2) * $unit)),
        [System.Drawing.PointF]::new([single](($x2 - $nx * $r2) * $unit), [single](($y2 - $ny * $r2) * $unit)),
        [System.Drawing.PointF]::new([single](($x1 - $nx * $r1) * $unit), [single](($y1 - $ny * $r1) * $unit))
    )
    $graphics.FillPolygon($brush, $points)
    $graphics.FillEllipse($brush, [single](($x1 - $r1) * $unit), [single](($y1 - $r1) * $unit), [single](2 * $r1 * $unit), [single](2 * $r1 * $unit))
    $graphics.FillEllipse($brush, [single](($x2 - $r2) * $unit), [single](($y2 - $r2) * $unit), [single](2 * $r2 * $unit), [single](2 * $r2 * $unit))
}

function Add-RoundedRect($graphics, $brush, $unit, $x, $y, $w, $h, $r) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = 2 * $r * $unit
    $left = $x * $unit; $top = $y * $unit; $right = ($x + $w) * $unit; $bottom = ($y + $h) * $unit
    $path.AddArc($left, $top, $d, $d, 180, 90)
    $path.AddArc($right - $d, $top, $d, $d, 270, 90)
    $path.AddArc($right - $d, $bottom - $d, $d, $d, 0, 90)
    $path.AddArc($left, $bottom - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    $graphics.FillPath($brush, $path)
    $path.Dispose()
}

function New-IconImage([int]$size) {
    $big = $size * $supersample
    $source = New-Object System.Drawing.Bitmap $big, $big, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($source)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.Clear([System.Drawing.Color]::Transparent)
    $unit = $big / 32.0
    $borderBrush = New-Object System.Drawing.SolidBrush $border
    $paneBrush = New-Object System.Drawing.SolidBrush $pane
    $penBrush = New-Object System.Drawing.SolidBrush $pen
    Add-RoundedRect $graphics $borderBrush $unit 0.5 0.5 31 31 7
    Add-RoundedRect $graphics $paneBrush $unit 1.6 1.6 28.8 28.8 6
    Add-Capsule $graphics $penBrush $unit 20.73 8.98 12.56 17.15 2.92 2.92
    Add-Capsule $graphics $penBrush $unit 12.56 17.15 9.29 21.19 2.92 0.52
    Add-Capsule $graphics $penBrush $unit 6.97 25.53 11.27 23.81 1.03 1.03
    Add-Capsule $graphics $penBrush $unit 11.27 23.81 15.57 25.53 1.03 1.03
    Add-Capsule $graphics $penBrush $unit 15.57 25.53 19.87 23.81 1.03 1.03
    Add-Capsule $graphics $penBrush $unit 19.87 23.81 24.17 25.53 1.03 1.03
    $graphics.Dispose()
    $target = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $reducer = [System.Drawing.Graphics]::FromImage($target)
    $reducer.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $reducer.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $reducer.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
    $reducer.DrawImage($source, 0, 0, $size, $size)
    $reducer.Dispose()
    $source.Dispose()
    $stream = New-Object System.IO.MemoryStream
    $target.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $target.Dispose()
    return , $stream.ToArray()
}

$images = foreach ($size in $sizes) { , (New-IconImage $size) }
$icon = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter $icon
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $dimension = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
    $writer.Write([byte]$dimension); $writer.Write([byte]$dimension)
    $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]1); $writer.Write([uint16]32)
    $writer.Write([uint32]$images[$i].Length); $writer.Write([uint32]$offset)
    $offset += $images[$i].Length
}
foreach ($image in $images) { $writer.Write([byte[]]$image) }
$writer.Flush()
New-Item -ItemType Directory -Force (Split-Path $Output) | Out-Null
[System.IO.File]::WriteAllBytes((Join-Path $root $Output), $icon.ToArray())
Write-Host ("{0}: {1} bytes, sizes {2}" -f $Output, $icon.Length, ($sizes -join ", "))
