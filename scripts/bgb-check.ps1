<#
.SYNOPSIS
Runs a ROM headless in BGB with every hardware-rule exception break on, and takes a screenshot.

.DESCRIPTION
Uses the project's own BGB in tools/bgb (its bgb.ini already has the breaks on, see Docs/DEBUGGING.md); the
settings are passed with -set as well, so the check holds even if that ini is changed. Headless mode never
writes the ini. Two runs:

1. Rule check: runs the ROM at full speed for -Seconds. BGB headless exits as soon as it would break to the
   debugger, so a ROM that is still running at the end broke no rule. Exiting early = FAIL.
2. Screenshot: breaks on the -Frames'th call of GBDK's _vsync and saves build/screens/bgb-<rom>-<mode>.png.

.EXAMPLE
.\scripts\bgb-check.ps1 build\gbc\doors.gbc
.\scripts\bgb-check.ps1 build\dmg\doors.gb -Mode gbc   # DMG ROM on a GBC (compatibility mode)
#>
param(
    [Parameter(Mandatory)] [string] $Rom,
    [ValidateSet('auto', 'dmg', 'gbc')] [string] $Mode = 'auto',
    [int] $Seconds = 10,
    [int] $Frames = 300,
    [string] $Bgb
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
# The project's own BGB (tools/bgb), never the owner's install
if (-not $Bgb) { $Bgb = Join-Path $root 'tools\bgb\bgb64.exe' }
$romPath = (Resolve-Path $Rom).Path
$shots = Join-Path $root 'build\screens'
New-Item -ItemType Directory -Force $shots | Out-Null

# Exceptions tab: break on echo RAM, inaccessible VRAM/OAM, LCD off outside VBlank, bad OAM DMA accesses,
# locked SRAM, uninitialised WRAM reads (WRAM starts random like hardware), invalid opcodes.
$settings = 'DebugEcho=1', 'DebugVRAMbreak=1', 'DebugDisableLCD=1', 'DebugOAMDMABreak=1', 'DebugSramBreak=1',
            'DebugWRAMbreak=1', 'UninitedWRAM=1', 'InvalidOpBreak=1', 'EmulateInaccVRAM=1'
# SystemMode as found by testing (Docs/DEBUGGING.md): 0 = DMG, 1 = GBC, 3 = automatic from the header
$settings += switch ($Mode) { 'dmg' { 'SystemMode=0' } 'gbc' { 'SystemMode=1' } default { 'SystemMode=3' } }
$common = @('-hf', '-nobatt')
foreach ($s in $settings) { $common += '-set', $s }

function Invoke-Bgb([string[]] $extra, [int] $timeoutMs) {
    $p = Start-Process $Bgb -ArgumentList ($common + $extra + @('-rom', "`"$romPath`"")) -PassThru
    if ($p.WaitForExit($timeoutMs)) { return $true }
    $p.Kill(); $p.WaitForExit()
    return $false
}

# e.g. bgb-gbc-autotest-gbc: build folder, then the console it ran as
$name = "bgb-$(Split-Path (Split-Path $romPath -Parent) -Leaf)-$Mode"

# 1. Rule check
$bmp = Join-Path $shots "$name-FAIL.bmp"
Remove-Item $bmp -ErrorAction SilentlyContinue
$exited = Invoke-Bgb @('-screenonexit', "`"$bmp`"") ($Seconds * 1000)
if ($exited) {
    Write-Host "FAIL: BGB broke to the debugger within $Seconds s (hardware rule broken). Screen at exit: $bmp"
    Write-Host "      Run it in BGB with the debugger to see where: Options > Exceptions, same boxes."
    exit 1
}
Write-Host "OK: ran $Seconds s at full speed with every exception break on ($romPath, mode $Mode)"

# 2. Screenshot
$bmp = Join-Path $shots "$name.bmp"
Remove-Item $bmp -ErrorAction SilentlyContinue
if (-not (Invoke-Bgb @('-br', "_vsync/..$Frames", '-screenonexit', "`"$bmp`"") 20000) -or -not (Test-Path $bmp)) {
    Write-Host "WARN: no screenshot (is _vsync in the .sym file next to the ROM?)"
    exit 0
}
Add-Type -AssemblyName System.Drawing
$img = [System.Drawing.Image]::FromFile($bmp)
$png = [IO.Path]::ChangeExtension($bmp, '.png')
$img.Save($png, [System.Drawing.Imaging.ImageFormat]::Png)
$img.Dispose()
Remove-Item $bmp
Write-Host "Screenshot after $Frames frames: $png"
