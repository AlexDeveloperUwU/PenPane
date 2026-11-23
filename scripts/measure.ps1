# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Measures idle CPU, memory, wakeups, leaks and startup time of PenPane.exe through the real executable.

.DESCRIPTION
Copies the executable into a scratch directory with a known PenPane.ini, drives it with synthetic input
(second launch toggles drawing, mouse strokes, hotkeys) and prints one table row per state.
Idle CPU is reported as TotalProcessorTime and QueryProcessCycleTime deltas over IdleSeconds,
together with context switch and thread counts. Memory comes from GetProcessMemoryInfo
(PROCESS_MEMORY_COUNTERS_EX2): working set, private working set and private bytes.

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\measure.ps1 -IdleSeconds 60

.EXAMPLE
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\measure.ps1 -Only startup,leak -Cycles 500
#>
param(
    [string]$Exe = "build\PenPane.exe",
    [int]$IdleSeconds = 60,
    [int]$SettleSeconds = 8,
    [int]$Strokes = 200,
    [int]$Cycles = 500,
    [int]$StartupRuns = 7,
    [string[]]$Only = @(),
    [string]$Json = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root

Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class PenPaneNative
{
    [StructLayout(LayoutKind.Sequential)]
    public struct MemoryCounters
    {
        public uint cb;
        public uint PageFaultCount;
        public UIntPtr PeakWorkingSetSize;
        public UIntPtr WorkingSetSize;
        public UIntPtr QuotaPeakPagedPoolUsage;
        public UIntPtr QuotaPagedPoolUsage;
        public UIntPtr QuotaPeakNonPagedPoolUsage;
        public UIntPtr QuotaNonPagedPoolUsage;
        public UIntPtr PagefileUsage;
        public UIntPtr PeakPagefileUsage;
        public UIntPtr PrivateUsage;
        public UIntPtr PrivateWorkingSetSize;
        public ulong SharedCommitUsage;
    }
    [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll")] public static extern bool QueryProcessCycleTime(IntPtr process, out ulong cycles);
    [DllImport("kernel32.dll")] public static extern bool K32GetProcessMemoryInfo(IntPtr process, ref MemoryCounters counters, uint size);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
    [DllImport("user32.dll")] public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr param);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    public delegate bool EnumProc(IntPtr window, IntPtr param);

    public static int VisibleWindowCount(uint pid)
    {
        int count = 0;
        EnumWindows(delegate (IntPtr window, IntPtr param)
        {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner == pid && IsWindowVisible(window)) { count++; }
            return true;
        }, IntPtr.Zero);
        return count;
    }
}
"@

$scratch = Join-Path $env:TEMP "penpane-measure"
$screenWidth = [PenPaneNative]::GetSystemMetrics(0)
$screenHeight = [PenPaneNative]::GetSystemMetrics(1)
$random = New-Object System.Random 7

function Stop-PenPane {
    Get-Process penpane -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 300
}

function Initialize-Scratch {
    Stop-PenPane
    if (Test-Path $scratch) { Remove-Item $scratch -Recurse -Force }
    New-Item -ItemType Directory $scratch | Out-Null
    $source = if ([IO.Path]::IsPathRooted($Exe)) { $Exe } else { Join-Path $root $Exe }
    Copy-Item $source (Join-Path $scratch "PenPane.exe")
    Set-Content -Path (Join-Path $scratch "PenPane.ini") -Value "tool_lock = true`r`nbind = F22 single clear`r`n"
}

function Start-PenPane {
    $process = [Diagnostics.Process]::Start((Join-Path $scratch "PenPane.exe"))
    Start-Sleep -Milliseconds 1500
    return $process
}

function Invoke-SecondLaunch {
    Start-Process (Join-Path $scratch "PenPane.exe") | Out-Null
    Start-Sleep -Milliseconds 900
}

function Send-Key([byte[]]$keys, [int]$holdMs = 20) {
    foreach ($key in $keys) { [PenPaneNative]::keybd_event($key, 0, 0, [UIntPtr]::Zero) }
    Start-Sleep -Milliseconds $holdMs
    [array]::Reverse($keys)
    foreach ($key in $keys) { [PenPaneNative]::keybd_event($key, 0, 2, [UIntPtr]::Zero) }
}

function Send-Undo { Send-Key ([byte[]](0x11, 0x5A)) 5 }
function Send-Clear { Send-Key ([byte[]](0x85)) 20; Start-Sleep -Milliseconds 120 }
function Send-ToggleMode { Send-Key ([byte[]](0x7D)) 20; Start-Sleep -Milliseconds 60; Send-Key ([byte[]](0x7D)) 20; Start-Sleep -Milliseconds 900 }

function Move-Stroke([int]$x0, [int]$y0, [int]$points = 24, [int]$stepDelayMs = 2, [int]$amplitude = 40, [int]$stepX = 12) {
    [PenPaneNative]::SetCursorPos($x0, $y0) | Out-Null
    Start-Sleep -Milliseconds 4
    [PenPaneNative]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    for ($i = 1; $i -le $points; $i++) {
        $x = $x0 + $i * $stepX
        $y = $y0 + [int]([math]::Sin($i / 3.0) * $amplitude)
        [PenPaneNative]::SetCursorPos($x, $y) | Out-Null
        Start-Sleep -Milliseconds $stepDelayMs
    }
    [PenPaneNative]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 6
}

function Move-RandomStroke {
    $x = 200 + $random.Next(0, $screenWidth - 700)
    $y = 150 + $random.Next(0, $screenHeight - 400)
    Move-Stroke $x $y
}

function Get-Memory($process) {
    $handle = [PenPaneNative]::OpenProcess(0x1010, $false, [uint32]$process.Id)
    $counters = New-Object PenPaneNative+MemoryCounters
    $counters.cb = [Runtime.InteropServices.Marshal]::SizeOf($counters)
    [void][PenPaneNative]::K32GetProcessMemoryInfo($handle, [ref]$counters, $counters.cb)
    [void][PenPaneNative]::CloseHandle($handle)
    return [pscustomobject]@{
        WorkingSetMB = [math]::Round($counters.WorkingSetSize.ToUInt64() / 1MB, 2)
        PrivateWorkingSetMB = [math]::Round($counters.PrivateWorkingSetSize.ToUInt64() / 1MB, 2)
        PrivateBytesMB = [math]::Round($counters.PrivateUsage.ToUInt64() / 1MB, 2)
    }
}

function Get-Activity($process) {
    $process.Refresh()
    $handle = [PenPaneNative]::OpenProcess(0x1000, $false, [uint32]$process.Id)
    $cycles = [uint64]0
    [void][PenPaneNative]::QueryProcessCycleTime($handle, [ref]$cycles)
    [void][PenPaneNative]::CloseHandle($handle)
    $perThread = @{}
    foreach ($thread in Get-CimInstance Win32_PerfRawData_PerfProc_Thread -Filter "IDProcess=$($process.Id)") {
        $perThread[[string]$thread.IDThread] = [uint64]$thread.ContextSwitchesPersec
    }
    return [pscustomobject]@{
        CpuMs = $process.TotalProcessorTime.TotalMilliseconds
        Cycles = $cycles
        Switches = $perThread
        Threads = $process.Threads.Count
    }
}

function Get-SwitchDelta($before, $after) {
    $total = [int64]0
    foreach ($id in $after.Keys) {
        $start = if ($before.ContainsKey($id)) { $before[$id] } else { [uint64]0 }
        $total += [int64]($after[$id] - $start)
    }
    return $total
}

function Measure-State([string]$name, $process, [int]$seconds) {
    Start-Sleep -Seconds $SettleSeconds
    $memory = Get-Memory $process
    $before = Get-Activity $process
    Start-Sleep -Seconds $seconds
    $after = Get-Activity $process
    $row = [pscustomobject][ordered]@{
        State = $name
        CpuDeltaMs = [math]::Round($after.CpuMs - $before.CpuMs, 3)
        CycleDelta = [uint64]($after.Cycles - $before.Cycles)
        CtxSwitches = (Get-SwitchDelta $before.Switches $after.Switches)
        Threads = $after.Threads
        WorkingSetMB = $memory.WorkingSetMB
        PrivateWSMB = $memory.PrivateWorkingSetMB
        PrivateBytesMB = $memory.PrivateBytesMB
    }
    $script:rows += $row
    Write-Host ("{0,-44} cpu {1,8} ms  cycles {2,12}  ctx {3,5}  thr {4,2}  WS {5,7}  PWS {6,7}  PB {7,7}" -f $row.State, $row.CpuDeltaMs, $row.CycleDelta, $row.CtxSwitches, $row.Threads, $row.WorkingSetMB, $row.PrivateWSMB, $row.PrivateBytesMB)
}

function Measure-Snapshot([string]$name, $process) {
    Start-Sleep -Milliseconds 800
    $memory = Get-Memory $process
    $row = [pscustomobject][ordered]@{
        State = $name
        CpuDeltaMs = $null
        CycleDelta = $null
        CtxSwitches = $null
        Threads = $process.Threads.Count
        WorkingSetMB = $memory.WorkingSetMB
        PrivateWSMB = $memory.PrivateWorkingSetMB
        PrivateBytesMB = $memory.PrivateBytesMB
    }
    $script:rows += $row
    Write-Host ("{0,-44} WS {1,7}  PWS {2,7}  PB {3,7}" -f $row.State, $row.WorkingSetMB, $row.PrivateWSMB, $row.PrivateBytesMB)
}

function Wanted([string]$name) {
    $names = @($Only | ForEach-Object { $_ -split "," } | Where-Object { $_ })
    return ($names.Count -eq 0) -or ($names -contains $name)
}

function Test-ClickThrough {
    $form = $null
    Add-Type -AssemblyName System.Windows.Forms
    $script:clicks = 0
    $form = New-Object System.Windows.Forms.Form
    $form.FormBorderStyle = "None"
    $form.StartPosition = "Manual"
    $form.Location = New-Object System.Drawing.Point(1200, 600)
    $form.Size = New-Object System.Drawing.Size(300, 300)
    $form.TopMost = $false
    $form.Add_MouseDown({ $script:clicks++ })
    $form.Show()
    Update-Forms
    return $form
}

function Update-Forms {
    for ($i = 0; $i -lt 8; $i++) {
        [System.Windows.Forms.Application]::DoEvents()
        Start-Sleep -Milliseconds 40
    }
}

function Send-Click([int]$x, [int]$y) {
    [PenPaneNative]::SetCursorPos($x, $y) | Out-Null
    Start-Sleep -Milliseconds 30
    [PenPaneNative]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    [PenPaneNative]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    Update-Forms
}

function Measure-Startup {
    $samples = @()
    for ($run = 0; $run -lt $StartupRuns; $run++) {
        Stop-PenPane
        $watch = [Diagnostics.Stopwatch]::StartNew()
        $process = [Diagnostics.Process]::Start((Join-Path $scratch "PenPane.exe"))
        $visible = $null
        while ($watch.ElapsedMilliseconds -lt 5000) {
            if ([PenPaneNative]::VisibleWindowCount([uint32]$process.Id) -gt 0) { $visible = $watch.Elapsed.TotalMilliseconds; break }
            [Threading.Thread]::Sleep(1)
        }
        if ($visible -ne $null) { $samples += $visible }
        Start-Sleep -Milliseconds 500
    }
    Stop-PenPane
    $sorted = $samples | Sort-Object
    $median = if ($sorted.Count) { $sorted[[int][math]::Floor($sorted.Count / 2)] } else { $null }
    Write-Host ("startup to first visible window: median {0:N1} ms  min {1:N1}  max {2:N1}  samples {3}" -f $median, $sorted[0], $sorted[-1], $sorted.Count)
    $script:summary["startup_ms_median"] = [math]::Round($median, 1)
}

function Measure-LongStroke($process) {
    $before = Get-Activity $process
    $watch = [Diagnostics.Stopwatch]::StartNew()
    [PenPaneNative]::SetCursorPos(300, 400) | Out-Null
    Start-Sleep -Milliseconds 10
    [PenPaneNative]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    $points = 2000
    for ($i = 1; $i -le $points; $i++) {
        $angle = $i / 40.0
        $x = 1280 + [int]([math]::Cos($angle) * (200 + $i / 5.0))
        $y = 700 + [int]([math]::Sin($angle * 1.3) * (150 + $i / 12.0))
        [PenPaneNative]::SetCursorPos($x, $y) | Out-Null
        [Threading.Thread]::Sleep(1)
    }
    [PenPaneNative]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 300
    $after = Get-Activity $process
    $watch.Stop()
    $cpu = $after.CpuMs - $before.CpuMs
    Write-Host ("long stroke ({0} moves, {1:N0} ms wall): cpu {2:N1} ms  cycles {3}" -f $points, $watch.Elapsed.TotalMilliseconds, $cpu, ($after.Cycles - $before.Cycles))
    $script:summary["long_stroke_cpu_ms"] = [math]::Round($cpu, 1)
    $script:summary["long_stroke_cycles"] = [uint64]($after.Cycles - $before.Cycles)
}

$script:rows = @()
$script:summary = [ordered]@{}
Initialize-Scratch
Write-Host ("exe: {0} bytes  screen {1}x{2}" -f (Get-Item (Join-Path $scratch "PenPane.exe")).Length, $screenWidth, $screenHeight)
$script:summary["exe_bytes"] = (Get-Item (Join-Path $scratch "PenPane.exe")).Length

try {
    if (Wanted "startup") { Measure-Startup }

    if (Wanted "states") {
        Initialize-Scratch
        $form = Test-ClickThrough
        $process = Start-PenPane
        Measure-Snapshot "just started (hidden), +1.5 s" $process
        Start-Sleep -Seconds 14
        Measure-State "hidden, toolbar handle only" $process $IdleSeconds
        $modules = ($process.Modules | ForEach-Object { $_.ModuleName.ToLower() } | Sort-Object) -join ","
        $script:summary["modules"] = $modules
        Write-Host "modules: $modules"

        Invoke-SecondLaunch
        Measure-State "drawing, empty desktop" $process $IdleSeconds

        Move-Stroke 400 500
        Move-Stroke 400 700
        Measure-State "drawing, 2 strokes" $process $IdleSeconds

        Send-Key ([byte[]](0x4B)) 20
        Start-Sleep -Milliseconds 300
        Move-Stroke 600 400 30 8
        Send-Key ([byte[]](0x50)) 20
        Start-Sleep -Milliseconds 2500
        Measure-State "drawing, after laser faded" $process $IdleSeconds

        Send-Key ([byte[]](0x4B)) 20
        Start-Sleep -Milliseconds 300
        [PenPaneNative]::SetCursorPos(600, 900) | Out-Null
        [PenPaneNative]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
        for ($i = 0; $i -lt 40; $i++) { [PenPaneNative]::SetCursorPos(600 + $i * 10, 900 + [int]([math]::Sin($i / 4.0) * 30)) | Out-Null; Start-Sleep -Milliseconds 8 }
        Start-Sleep -Milliseconds 100
        Measure-Snapshot "laser active (button held, trail alive)" $process
        [PenPaneNative]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 2500
        Send-Key ([byte[]](0x50)) 20

        Invoke-SecondLaunch
        Measure-State "passthrough with annotations" $process $IdleSeconds

        Send-Click 1300 700
        $passthroughClicks = $script:clicks
        Invoke-SecondLaunch
        $clicksBefore = $script:clicks
        Send-Click 1300 720
        $capturedClicks = $script:clicks - $clicksBefore
        $form.Close()
        Write-Host ("click-through check: passthrough reached app {0} click(s) (expect 1); drawing reached app {1} (expect 0)" -f $passthroughClicks, $capturedClicks)
        $script:summary["click_through_ok"] = ($passthroughClicks -eq 1 -and $capturedClicks -eq 0)

        Send-Clear
        Send-ToggleMode
        Measure-State "whiteboard, empty" $process $IdleSeconds
        Move-Stroke 500 500
        Measure-State "whiteboard, 1 stroke" $process $IdleSeconds

        Invoke-SecondLaunch
        Measure-State "whiteboard, drawing off (overlay destroyed)" $process $IdleSeconds

        Invoke-SecondLaunch
        Send-ToggleMode
        Invoke-SecondLaunch
        Measure-State "desktop, drawing off, empty scene" $process $IdleSeconds
        Stop-PenPane
    }

    if (Wanted "strokes") {
        Initialize-Scratch
        $process = Start-PenPane
        Invoke-SecondLaunch
        Measure-Snapshot "drawing, empty (baseline for strokes)" $process
        for ($i = 0; $i -lt $Strokes; $i++) { Move-RandomStroke }
        Measure-State "drawing after $Strokes strokes" $process $IdleSeconds
        Invoke-SecondLaunch
        Measure-State "passthrough with $Strokes strokes" $process $IdleSeconds
        Invoke-SecondLaunch
        for ($i = 0; $i -lt $Strokes; $i++) { Send-Undo }
        Measure-Snapshot "after $Strokes undos" $process
        for ($i = 0; $i -lt 20; $i++) { Move-RandomStroke }
        Send-Clear
        Measure-Snapshot "after clear" $process
        Stop-PenPane
    }

    if (Wanted "longstroke") {
        Initialize-Scratch
        $process = Start-PenPane
        Invoke-SecondLaunch
        Measure-LongStroke $process
        Stop-PenPane
    }

    if (Wanted "leak") {
        Initialize-Scratch
        $process = Start-PenPane
        Invoke-SecondLaunch
        for ($i = 0; $i -lt 5; $i++) { Move-RandomStroke; Send-Undo }
        Send-Clear
        Measure-Snapshot "leak: baseline (warm, empty)" $process
        $baseline = Get-Memory $process
        for ($cycle = 1; $cycle -le $Cycles; $cycle++) {
            Move-RandomStroke
            Move-RandomStroke
            Send-Clear
            Send-Undo
            Send-Undo
            Send-Undo
            if ($cycle % 100 -eq 0) {
                $memory = Get-Memory $process
                Write-Host ("  cycle {0}: WS {1} PWS {2} PB {3}" -f $cycle, $memory.WorkingSetMB, $memory.PrivateWorkingSetMB, $memory.PrivateBytesMB)
            }
        }
        Send-Clear
        Send-Undo
        Send-Undo
        Send-Undo
        Send-Clear
        Measure-Snapshot "leak: after $Cycles cycles + clear" $process
        $final = Get-Memory $process
        $script:summary["leak_private_bytes_delta_mb"] = [math]::Round($final.PrivateBytesMB - $baseline.PrivateBytesMB, 2)
        $script:summary["leak_private_ws_delta_mb"] = [math]::Round($final.PrivateWorkingSetMB - $baseline.PrivateWorkingSetMB, 2)
        Stop-PenPane
    }

    if (Wanted "exit") {
        Initialize-Scratch
        $process = Start-PenPane
        Send-Key ([byte[]](0x11, 0x12, 0x10, 0x1B)) 50
        Start-Sleep -Milliseconds 800
        $script:summary["emergency_exit_ok"] = [bool]$process.HasExited
        Write-Host ("emergency exit: process exited = {0}" -f $process.HasExited)
    }
}
finally {
    Stop-PenPane
}

$script:rows | Format-Table -AutoSize | Out-String -Width 220 | Write-Host
$script:summary.GetEnumerator() | ForEach-Object { Write-Host ("{0} = {1}" -f $_.Key, $_.Value) }
if ($Json) {
    [pscustomobject]@{ rows = $script:rows; summary = $script:summary } | ConvertTo-Json -Depth 4 | Set-Content $Json -Encoding UTF8
}






